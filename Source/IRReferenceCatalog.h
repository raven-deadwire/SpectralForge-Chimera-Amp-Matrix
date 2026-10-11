#pragma once
namespace spectralforge {
// Public 1.3 builds expose only the two attributed, hash-pinned factory IRs.
// Unapproved development/reference capture inventories are not shipped or
// advertised by the loader. User-selected files and their own sidecars remain
// independent of this catalog; saved source values 0/1/2/3 are unchanged.
inline constexpr int publicIRFactoryCount=2;
inline constexpr const char* referenceIRCatalog=R"IRCAT([])IRCAT";
inline constexpr const char* externalBassIRCatalog=R"IRCAT([])IRCAT";
inline constexpr const char* ravenIRCatalog=R"IRCAT([])IRCAT";
}
