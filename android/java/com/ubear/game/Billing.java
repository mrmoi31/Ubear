package com.ubear.game;

import android.app.Activity;

import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;

/**
 * Achats intégrés Google Play pour le C++ (Platform.cpp).
 * Les réponses de Play arrivent sur d'autres threads : elles sont mises en file
 * et le jeu les lit à chaque image avec poll() -> {type, produit, donnée}.
 * Types : "price" (prix affiché), "purchased" (jeton à rendre à finish()),
 * "pending" (paiement en attente), "failed" ("cancel" ou code d'erreur).
 */
public final class Billing {
    private static final ConcurrentLinkedQueue<String[]> events = new ConcurrentLinkedQueue<>();
    private static final Map<String, ProductDetails> products = new HashMap<>();
    private static final Map<String, Purchase> unfinished = new HashMap<>();   // jeton -> achat pas encore livré
    private static BillingClient client;
    private static Activity activity;
    private static String[] productIds;

    private Billing() {}

    public static synchronized void start(Activity act, String[] ids) {
        if (client != null) return;
        activity = act;
        productIds = ids;
        client = BillingClient.newBuilder(act.getApplicationContext())
                .setListener(Billing::onPurchasesUpdated)
                .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
                .enableAutoServiceReconnection()
                .build();
        client.startConnection(new BillingClientStateListener() {
            @Override public void onBillingSetupFinished(BillingResult r) {
                if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
                queryProducts();
                restore();   // achats payés mais pas livrés (appli tuée, paiement en attente validé)
            }
            @Override public void onBillingServiceDisconnected() {}   // reconnexion automatique
        });
    }

    private static void queryProducts() {
        List<QueryProductDetailsParams.Product> list = new ArrayList<>();
        for (String id : productIds) {
            list.add(QueryProductDetailsParams.Product.newBuilder()
                    .setProductId(id).setProductType(BillingClient.ProductType.INAPP).build());
        }
        client.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(list).build(), (r, res) -> {
            if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
            for (ProductDetails d : res.getProductDetailsList()) {
                synchronized (Billing.class) { products.put(d.getProductId(), d); }
                ProductDetails.OneTimePurchaseOfferDetails o = d.getOneTimePurchaseOfferDetails();
                if (o != null) events.add(new String[]{"price", d.getProductId(), o.getFormattedPrice()});
            }
        });
    }

    public static void buy(String id) {
        ProductDetails d;
        synchronized (Billing.class) { d = products.get(id); }
        if (client == null || d == null) {
            events.add(new String[]{"failed", id, "unavailable"});   // Play absent, hors ligne ou produit inconnu
            if (client != null) queryProducts();
            return;
        }
        BillingFlowParams params = BillingFlowParams.newBuilder()
                .setProductDetailsParamsList(Collections.singletonList(
                        BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(d).build()))
                .build();
        activity.runOnUiThread(() -> {
            BillingResult r = client.launchBillingFlow(activity, params);
            if (r.getResponseCode() != BillingClient.BillingResponseCode.OK)
                events.add(new String[]{"failed", id, String.valueOf(r.getResponseCode())});
        });
    }

    private static void onPurchasesUpdated(BillingResult r, List<Purchase> list) {
        int code = r.getResponseCode();
        if (code == BillingClient.BillingResponseCode.OK && list != null) {
            for (Purchase p : list) handle(p);
        } else if (code == BillingClient.BillingResponseCode.USER_CANCELED) {
            events.add(new String[]{"failed", "", "cancel"});
        } else if (code == BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED) {
            restore();   // déjà payé (ex. remove_ads sur un autre appareil) : on le livre
        } else {
            events.add(new String[]{"failed", "", String.valueOf(code)});
        }
    }

    private static void handle(Purchase p) {
        if (p.getPurchaseState() == Purchase.PurchaseState.PENDING) {
            for (String id : p.getProducts()) events.add(new String[]{"pending", id, ""});
            return;
        }
        if (p.getPurchaseState() != Purchase.PurchaseState.PURCHASED) return;
        synchronized (Billing.class) {
            if (unfinished.containsKey(p.getPurchaseToken())) return;   // déjà signalé au jeu
            unfinished.put(p.getPurchaseToken(), p);
        }
        for (String id : p.getProducts()) events.add(new String[]{"purchased", id, p.getPurchaseToken()});
    }

    /** Appelé par le jeu une fois l'achat crédité et sauvegardé. */
    public static void finish(String token, boolean consumable) {
        Purchase p;
        synchronized (Billing.class) { p = unfinished.get(token); }
        if (client == null || p == null) return;
        // Retiré de la liste seulement en cas de succès : sinon restore() le redonnera au prochain lancement.
        // shortcut: si la consommation échoue, les pièces seront recréditées au lancement suivant (rare, en faveur du joueur).
        if (consumable) {
            client.consumeAsync(ConsumeParams.newBuilder().setPurchaseToken(token).build(), (r, t) -> done(token, r));
        } else if (!p.isAcknowledged()) {
            // Sans accusé de réception sous 3 jours, Google rembourse l'achat.
            client.acknowledgePurchase(AcknowledgePurchaseParams.newBuilder().setPurchaseToken(token).build(), r -> done(token, r));
        } else {
            synchronized (Billing.class) { unfinished.remove(token); }   // déjà acquitté : "Restaurer" pourra le redonner
        }
    }

    private static void done(String token, BillingResult r) {
        if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
        synchronized (Billing.class) { unfinished.remove(token); }
    }

    /** Redonne au jeu les achats non consommés et les non-consommables possédés. */
    public static void restore() {
        if (client == null) return;
        client.queryPurchasesAsync(QueryPurchasesParams.newBuilder().setProductType(BillingClient.ProductType.INAPP).build(),
                (r, list) -> {
                    if (r.getResponseCode() != BillingClient.BillingResponseCode.OK) return;
                    for (Purchase p : list) handle(p);
                });
    }

    public static String[] poll() {
        return events.poll();
    }
}
