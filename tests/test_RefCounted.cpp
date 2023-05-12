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

#include <cstdlib>
#include <string>
#include <CppUnitXLite/CppUnitXLite.cpp>
#include "thekogans/util/Types.h"
#include "thekogans/util/RefCounted.h"

using namespace thekogans;

struct RefCountedTest : public util::RefCounted {
protected:
    virtual void Harakiri () {
    }
};

TEST (thekogans, test_RefCounted) {
    RefCountedTest test;
    {
        std::cout << "RefCounted init...";
        bool result = test.GetRefCount () == 0;
        std::cout << (result ? "pass" : "fail") << std::endl;
        CHECK_EQUAL (result, true);
    }
    {
        std::cout << "RefCounted::AddRef...";
        bool result = test.AddRef () == 1;
        std::cout << (result ? "pass" : "fail") << std::endl;
        CHECK_EQUAL (result, true);
    }
    {
        std::cout << "RefCounted::Release...";
        bool result = test.Release () == 0;
        std::cout << (result ? "pass" : "fail") << std::endl;
        CHECK_EQUAL (result, true);
    }
    {
    }
}

TEST (thekogans, test_RefCounted_SharedPtr) {
}

TEST (thekogans, test_RefCounted_WeakPtr) {
}

TESTMAIN
