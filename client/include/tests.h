#pragma once

void print_test_outcome(int ret, char* test_name);

int FlsAlloc_test_01();
int FlsAlloc_test_02();

int FlsFree_test_01();
int FlsFree_test_02();
int FlsFree_test_03();

int FlsFree_test_BVA();

int tests_with_fiber();

void Fls_Alloc_Free_Test();

int flsAllocSetGetFree();

int flsAlloc_Until_err();
