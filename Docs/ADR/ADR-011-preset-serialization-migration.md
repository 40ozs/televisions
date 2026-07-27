# ADR-011: Preset serialization and migration
Context: Factory/user presets, versioning, corruption safety, forward compatibility.
Decision: Presets and session state share one schema: APVTS ValueTree + stateVersion property, serialized as XML (user files `.vfapreset`) and compiled-in strings (factory). Loader: parse-fail -> default state + flag; unknown params ignored; missing -> defaults; values clamped to manifest ranges; migrations keyed on stateVersion in one table.
Alternatives: JSON custom schema (loses APVTS integration); binary blobs (opaque, migration-hostile).
Consequences: One code path tests both presets and sessions.
Validation: State suite (SaveLoad/Preset/Migration tests incl. corrupted input).
Reversal cost: Medium once user presets exist in the wild — hence versioned from day one.
