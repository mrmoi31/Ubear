# Construit le jeu Android avec Gradle (android/) :
#   UBear.apk : installable directement sur un téléphone (adb install -r UBear.apk)
#   UBear.aab : à envoyer sur la Google Play Console
# Signature : android/keystore.properties (clé d'upload), sinon clé de debug.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$jdk = (Get-ChildItem "C:\Program Files\Eclipse Adoptium" -Directory | Sort-Object Name -Descending | Select-Object -First 1).FullName
$env:JAVA_HOME = $jdk
$env:ANDROID_HOME = $sdk

# Emplacement du SDK pour Gradle (fichier local, hors Git)
$local = "$root\android\local.properties"
if (-not (Test-Path $local)) { "sdk.dir=$($sdk.Replace('\', '/'))" | Set-Content -Encoding ascii $local }
if (-not (Test-Path "$root\android\keystore.properties")) {
    Write-Warning "android/keystore.properties absent : signature de debug, l'AAB sera refusé par Google Play."
}

# Sauvegardes de test du PC : ne doivent pas partir dans le paquet (aussi filtrées par Gradle)
Get-ChildItem "$root\assets" -Recurse -Filter "ubear_save*.txt" | Remove-Item -Force

Push-Location "$root\android"
try {
    & .\gradlew.bat --no-daemon assembleRelease bundleRelease
    if ($LASTEXITCODE -ne 0) { throw "Echec Gradle ($LASTEXITCODE)" }
} finally { Pop-Location }

Copy-Item "$root\android\app\build\outputs\apk\release\app-release.apk" "$root\UBear.apk" -Force
Copy-Item "$root\android\app\build\outputs\bundle\release\app-release.aab" "$root\UBear.aab" -Force
foreach ($f in "UBear.apk", "UBear.aab") {
    Write-Host "$f pret : $root\$f ($([math]::Round((Get-Item "$root\$f").Length / 1MB, 1)) Mo)"
}
