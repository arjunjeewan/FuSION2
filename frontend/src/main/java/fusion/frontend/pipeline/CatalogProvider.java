package fusion.frontend.pipeline;

/** Gives the catalog; implemented by {@code runner}, which asks {@code fusion --commands} (10.C11). */
public interface CatalogProvider {
    Catalog catalog();
}
