# Historical transfer artifacts

The readable bundles and root `pke_native_source.zip.b64.part*` chunks are retained only as historical transfers. They are not build inputs.

Build the tracked `native/` tree directly. It was imported from the verified `PKE_Native_Prototype_0_1_SAVE_MIGRATION_SOURCE.zip` baseline (SHA-256 `eadd960b38c763bf79651cb272748f04a67c7c044f0e9a34e022eefdc80bf622`), including its original tests and fixtures.

The old base64 transfer fails ZIP integrity checks. The readable transfer also contains an invalid quote in the save serializer; the original source does not. Do not reconstruct or patch product source from these artifacts during builds.
