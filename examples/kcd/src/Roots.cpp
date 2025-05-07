// Copyright 2011 Boris Kogan (boris@thekogans.net)
//
// This file is part of libthekogans_util.
//
// libthekogans_util is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// libthekogans_util is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with libthekogans_util. If not, see <http://www.gnu.org/licenses/>.

#include <string>
#include <list>
#include <vector>
#include <unordered_set>
#include "thekogans/util/Environment.h"
#include "thekogans/util/Path.h"
#include "thekogans/util/StringUtils.h"
#include "thekogans/util/FileAllocator.h"
#include "thekogans/util/FileAllocatorRegistry.h"
#include "thekogans/util/BTreeValues.h"
#include "thekogans/kcd/Roots.h"

namespace thekogans {
    namespace kcd {

        THEKOGANS_UTIL_IMPLEMENT_SERIALIZABLE (Roots, 1, util::BTree::Value::TYPE)

        void Roots::ScanRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator) {
            Root::SharedPtr root;
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    root = value[i];
                    break;
                }
            }
            util::FileAllocator::Transaction transaction (fileAllocator);
            bool created = false;
            if (root == nullptr) {
                root.Reset (new Root (path));
                created = true;
            }
            root->Scan (fileAllocator);
            if (created) {
                value.push_back (root);
            }
            fileAllocator->GetRegistry ().SetValue ("roots", this);
            transaction.Commit ();
        }

        void Roots::EnableRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    if (!value[i]->IsActive ()) {
                        util::FileAllocator::Transaction transaction (fileAllocator);
                        value[i]->SetActive (true);
                        fileAllocator->GetRegistry ().SetValue ("roots", this);
                        transaction.Commit ();
                    }
                    break;
                }
            }
        }

        void Roots::DisableRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    if (value[i]->IsActive ()) {
                        util::FileAllocator::Transaction transaction (fileAllocator);
                        value[i]->SetActive (false);
                        fileAllocator->GetRegistry ().SetValue ("roots", this);
                        transaction.Commit ();
                    }
                    break;
                }
            }
        }

        void Roots::DeleteRoot (
                const std::string &path,
                util::FileAllocator::SharedPtr fileAllocator) {
            for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                if (value[i]->GetPath () == path) {
                    util::FileAllocator::Transaction transaction (fileAllocator);
                    value[i]->Delete (fileAllocator);
                    value.erase (value.begin () + i);
                    fileAllocator->GetRegistry ().SetValue ("roots", this);
                    transaction.Commit ();
                    break;
                }
            }
        }

        void Roots::Find (
                util::FileAllocator::SharedPtr fileAllocator,
                const std::string &pattern,
                bool ignoreCase,
                bool ordered,
                std::vector<std::string> &results) {
            auto ScanPattern = [] (
                    std::list<std::string>::const_iterator pathBegin,
                    std::list<std::string>::const_iterator pathEnd,
                    std::list<std::string>::const_iterator patternBegin,
                    std::list<std::string>::const_iterator patternEnd,
                    bool ignoreCase,
                    bool ordered) -> bool {
                while (patternBegin != patternEnd) {
                    std::list<std::string>::const_iterator it =
                        FindPrefix (pathBegin, pathEnd, *patternBegin, ignoreCase);
                    if (it == pathEnd) {
                        return false;
                    }
                    // To honor ordered flag pattern components
                    // must come in order in the resulting paths.
                    if (ordered) {
                        pathBegin = ++it;
                    }
                    ++patternBegin;
                }
                return true;
            };
            std::list<std::string> patternComponents;
            util::Path (pattern).GetComponents (patternComponents);
            std::list<std::string>::const_iterator patternBegin = patternComponents.begin ();
            std::list<std::string>::const_iterator patternEnd = patternComponents.end ();
            if (patternBegin != patternEnd) {
                std::vector<std::string> paths;
                for (std::size_t i = 0, count = value.size (); i < count; ++i) {
                    if (value[i]->IsActive ()) {
                        value[i]->Find (fileAllocator, *patternBegin, ignoreCase, paths);
                    }
                }
                // If we don't care about order, or there's only one pattern
                // component, skip over the first pattern component because
                // value[i]->Find just found it.
                if (!ordered || std::next (patternBegin) == patternEnd) {
                    ++patternBegin;
                }
                // Multiple different components with the same prefix (Python/Python38-32)
                // can be found in the same path. Make sure we only count it once.
                std::unordered_set<std::string> duplicates;
                if (patternBegin == patternEnd) {
                    for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                        if (duplicates.insert (paths[i]).second) {
                            results.push_back (paths[i]);
                        }
                    }
                }
                else {
                    for (std::size_t i = 0, count = paths.size (); i < count; ++i) {
                        std::list<std::string> pathComponents;
                        util::Path (paths[i]).GetComponents (pathComponents);
                        if (ScanPattern (
                            #if defined (TOOLCHAIN_OS_Windows)
                                // If on Windows, skip over the drive leter.
                                ++pathComponents.begin (),
                            #else // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.begin (),
                            #endif // defined (TOOLCHAIN_OS_Windows)
                                pathComponents.end (),
                                patternBegin,
                                patternEnd,
                                ignoreCase,
                                ordered)) {
                            if (duplicates.insert (paths[i]).second) {
                                results.push_back (paths[i]);
                            }
                        }
                    }
                }
            }
        }


    } // namespace kcd
} // namespace thekogans
