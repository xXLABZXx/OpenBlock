// OpenBlock place files: JSON save/load for the DataModel tree.
// Clean-room code. Hand-rolled JSON writer/parser, no dependencies.
#pragma once

#include <string>

class DataModel;

// Saves every top-level child of `game` (including services) and its
// descendants to `path` as JSON. Each node is written as
// {"class":..., "name":..., "props":{...}, "children":[...]} with 2-space
// indentation. Returns true on success.
//
// Property notes:
//   - Only serializable property types are written: string, number, bool,
//     Vector3, Color3, CFrame ("CFrameV" is also accepted on load), UDim2V,
//     and BrickColorV (as {"number": n}).
//   - "Instance"-typed properties hold object references and cannot be
//     serialized, so they are skipped.
//   - "ClassName" and other read-only properties are skipped.
bool oboxSavePlace(DataModel* game, const std::string& path);

// Loads a place file written by oboxSavePlace into `game`. Top-level nodes
// whose class+name match an existing service are merged into that service;
// everything else is created fresh via DataModel::createInstance. Property
// problems are reported as warnings on stdout; loading continues.
// Returns true on success, false on file or JSON errors.
bool oboxLoadPlace(DataModel* game, const std::string& path);
