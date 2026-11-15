# Construit UBear.apk (NativeActivity, sans Java) : NDK -> aapt2 -> zipalign -> apksigner
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$ndk = (Get-ChildItem "$sdk\ndk" | Sort-Object Name -Descending | Select-Object -First 1).FullName
$bt = "$sdk\build-tools\36.0.0"
if (-not (Test-Path $bt)) { $bt = (Get-ChildItem "$sdk\build-tools" | Sort-Object Name -Descending | Select-Object -First 1).FullName }
$androidJar = "$sdk\platforms\android-34\android.jar"
$jdk = (Get-ChildItem "C:\Program Files\Eclipse Adoptium" -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$env:JAVA_HOME = $jdk
$env:PATH = "$jdk\bin;$env:PATH"

$abis = @("arm64-v8a", "armeabi-v7a", "x86_64")
$work = "$root\build\apk"
$outApk = "$root\UBear.apk"

function Run($exe, [string[]]$argv) {
    & $exe @argv
    if ($LASTEXITCODE -ne 0) { throw "Echec : $exe $($argv -join ' ')" }
}

# 1. Bibliothèques natives
foreach ($abi in $abis) {
    $bd = "$root\build\android-$abi"
    Run cmake @("-G", "Ninja", "-S", $root, "-B", $bd,
        "-DCMAKE_TOOLCHAIN_FILE=$ndk\build\cmake\android.toolchain.cmake",
        "-DANDROID_ABI=$abi", "-DANDROID_PLATFORM=android-24",
        "-DCMAKE_BUILD_TYPE=Release")
    Run cmake @("--build", $bd)
}

# 2. Ressources + manifeste + assets + focus audio
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory -Force "$work\res", "$work\classes" | Out-Null
Run "$jdk\bin\javac.exe" @("--release", "8", "-classpath", $androidJar, "-d", "$work\classes",
    "$root\android\java\com\ubear\game\AudioFocus.java")
Run "$bt\d8.bat" @("--min-api", "24", "--output", $work, "$work\classes\com\ubear\game\AudioFocus.class")
Get-ChildItem "$root\assets" -Recurse -Filter "ubear_save*.txt" | Remove-Item -Force
$stage = "$work\assets-pack"
New-Item -ItemType Directory -Force $stage | Out-Null
& robocopy "$root\assets" $stage /E /XD new-assets /XF "ubear_save*.txt" /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null
if ($LASTEXITCODE -ge 8) { throw "Copie des assets impossible ($LASTEXITCODE)" }
Run "$bt\aapt2.exe" @("compile", "--dir", "$root\android\app\src\main\res", "-o", "$work\res\res.zip")
Run "$bt\aapt2.exe" @("link", "-o", "$work\unsigned.apk", "-I", $androidJar,
    "--manifest", "$root\android\AndroidManifest.xml",
    "--min-sdk-version", "24", "--target-sdk-version", "34", "$work\res\res.zip")

# 3. Assets + .so
# Pas de `aapt2 link -A` : sous Windows il écrit les sous-dossiers avec des '\'
# (assets/cosmetics\hats\...), qu'Android ne retrouve pas. On les ajoute à la main en '/'.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open("$work\unsigned.apk", "Update")
$stageFull = (Resolve-Path $stage).Path.TrimEnd('\')
foreach ($f in Get-ChildItem $stage -Recurse -File) {
    $rel = $f.FullName.Substring($stageFull.Length + 1).Replace('\', '/')
    $level = if ($f.Extension -in @(".ttf", ".txt", ".json")) { [System.IO.Compression.CompressionLevel]::Optimal }
             else { [System.IO.Compression.CompressionLevel]::NoCompression }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $f.FullName, "assets/$rel", $level) | Out-Null
}
foreach ($abi in $abis) {
    $so = "$root\build\android-$abi\libubear.so"
    if (-not (Test-Path $so)) { throw "Manquant : $so" }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $so, "lib/$abi/libubear.so",
        [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
}
$dex = "$work\classes.dex"
if (-not (Test-Path $dex)) { throw "Manquant : $dex" }
[System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $dex, "classes.dex",
    [System.IO.Compression.CompressionLevel]::NoCompression) | Out-Null
$zip.Dispose()

# 4. Alignement + signature (keystore de debug)
Run "$bt\zipalign.exe" @("-f", "-p", "4", "$work\unsigned.apk", "$work\aligned.apk")
$ks = "$env:USERPROFILE\.android\debug.keystore"
if (-not (Test-Path $ks)) {
    New-Item -ItemType Directory -Force (Split-Path $ks) | Out-Null
    Run keytool @("-genkeypair", "-v", "-keystore", $ks, "-storepass", "android", "-alias", "androiddebugkey",
        "-keypass", "android", "-keyalg", "RSA", "-keysize", "2048", "-validity", "10000",
        "-dname", "CN=Android Debug,O=Android,C=US")
}
Run "$bt\apksigner.bat" @("sign", "--ks", $ks, "--ks-pass", "pass:android", "--key-pass", "pass:android",
    "--ks-key-alias", "androiddebugkey", "--out", $outApk, "$work\aligned.apk")
Run "$bt\apksigner.bat" @("verify", "--verbose", $outApk)
Write-Host "APK pret : $outApk ($([math]::Round((Get-Item $outApk).Length / 1MB, 1)) Mo)"
