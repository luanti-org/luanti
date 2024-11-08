// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2018 numzero, Lobachevskiy Vitaliy <numzer0@yandex.ru>

#include "test.h"

#include "exceptions.h"
#include "irr_ptr.h"
#include "IReferenceCounted.h"

class TestIrrPtr : public TestBase
{
public:
	TestIrrPtr() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestIrrPtr"; }

	void runTests(IGameDef *gamedef);

	void testRefCounting();
	void testSelfAssignment();
	void testNullHandling();
};

static TestIrrPtr g_test_instance;

void TestIrrPtr::runTests(IGameDef *gamedef)
{
	TEST(testRefCounting);
	TEST(testSelfAssignment);
	TEST(testNullHandling);
}

////////////////////////////////////////////////////////////////////////////////

#define UASSERT_REFERENCE_COUNT(object, value, info)                                     \
	UTEST((object)->getReferenceCount() == value,                                    \
			info "Reference count is %d instead of " #value,                 \
			(object)->getReferenceCount())

void TestIrrPtr::testRefCounting()
{
	IReferenceCounted *obj = new IReferenceCounted(); // RC=1
	obj->grab();
	UASSERT_REFERENCE_COUNT(obj, 2, "Pre-condition failed: ");
	{
		irr_ptr<IReferenceCounted> p1{obj}; // move semantics
		UASSERT(p1.get() == obj);
		UASSERT_REFERENCE_COUNT(obj, 2, );

		irr_ptr<IReferenceCounted> p2{p1}; // copy ctor
		UASSERT(p1.get() == obj);
		UASSERT(p2.get() == obj);
		UASSERT_REFERENCE_COUNT(obj, 3, );

		irr_ptr<IReferenceCounted> p3{std::move(p1)}; // move ctor
		UASSERT(p1.get() == nullptr);
		UASSERT(p3.get() == obj);
		UASSERT_REFERENCE_COUNT(obj, 3, );

		p1 = std::move(p2); // move assignment
		UASSERT(p1.get() == obj);
		UASSERT(p2.get() == nullptr);
		UASSERT_REFERENCE_COUNT(obj, 3, );

		p2 = p3; // copy assignment
		UASSERT(p2.get() == obj);
		UASSERT(p3.get() == obj);
		UASSERT_REFERENCE_COUNT(obj, 4, );

		p1.release();
		UASSERT(p1.get() == nullptr);
		UASSERT_REFERENCE_COUNT(obj, 4, );
	}
	UASSERT_REFERENCE_COUNT(obj, 2, );
	obj->drop();
	UTEST(obj->drop(), "Dropping failed: reference count is %d",
			obj->getReferenceCount());
}

#if defined(__clang__) || defined(__GNUC__)
	#pragma GCC diagnostic push
	#if __clang_major__ >= 7
		#pragma GCC diagnostic ignored "-Wself-assign-overloaded"
	#endif
	#if defined(__clang__) || __GNUC__ >= 13
		#pragma GCC diagnostic ignored "-Wself-move"
	#endif
#endif

void TestIrrPtr::testSelfAssignment()
{
	irr_ptr<IReferenceCounted> p1{new IReferenceCounted()};
	UASSERT(p1);
	UASSERT_REFERENCE_COUNT(p1, 1, );
	p1 = p1;
	UASSERT(p1);
	UASSERT_REFERENCE_COUNT(p1, 1, );
	p1 = std::move(p1);
	UASSERT(p1);
	UASSERT_REFERENCE_COUNT(p1, 1, );
}

void TestIrrPtr::testNullHandling()
{
	// In the case of an error, it will probably crash with SEGV.
	// Nevertheless, UASSERTs are used to catch possible corner cases.
	irr_ptr<IReferenceCounted> p1{new IReferenceCounted()};
	UASSERT(p1);
	irr_ptr<IReferenceCounted> p2;
	UASSERT(!p2);
	irr_ptr<IReferenceCounted> p3{p2};
	UASSERT(!p2);
	UASSERT(!p3);
	irr_ptr<IReferenceCounted> p4{std::move(p2)};
	UASSERT(!p2);
	UASSERT(!p4);
	p2 = p2;
	UASSERT(!p2);
	p2 = std::move(p2);
	UASSERT(!p2);
	p3 = p2;
	UASSERT(!p2);
	UASSERT(!p3);
	p3 = std::move(p2);
	UASSERT(!p2);
	UASSERT(!p3);
}

#if defined(__clang__) || defined(__GNUC__)
	#pragma GCC diagnostic pop
#endif
