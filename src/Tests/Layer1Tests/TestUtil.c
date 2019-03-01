#include "../UnitTests.h"
#include "../../util.h"

int* createStruct(int i){
    int* t = malloc(sizeof(int));
    *t = i;
    return t;
}

static ArrayList list = {0};
const int size = 10;
void populateList(void){
    for(int i = 0; i < size; i++)
        addToList(&list, createStruct(i));
}
void freeListMem(void){
    deleteList(&list);
    assert(getSize(&list) == 0);
}

START_TEST(test_arraylist){
    ArrayList list = {0};
    int size = 10;
    for(int n = 0; n < 5; n++){
        if(n < 3)
            for(int i = 0; i < size; i++){
                if(n == 2)
                    addUnique(&list, createStruct(i), sizeof(int));
                else addToList(&list, createStruct(i));
                assert(isNotEmpty(&list));
                assert(*(int*)getHead(&list) == 0);
            }
        else
            for(int i = size - 1; i >= 0; i--){
                prependToList(&list, createStruct(i));
                assert(isNotEmpty(&list));
                assert(*(int*)getHead(&list) == i);
            }
        assert(getSize(&list) == size);
        for(int i = 0; i < size; i++){
            assert(!addUnique(&list, &i, sizeof(int)));
            assert(getSize(&list) == size);
        }
        for(int i = 0; i < size; i++)
            assert(*(int*)getElement(&list, i) == i);
        deleteList(&list);
        assert(getSize(&list) == 0);
        assert(!isNotEmpty(&list));
    }
}
END_TEST

START_TEST(test_clearlist){
    clearList(&list);
    assert(getSize(&list) == 0);
    clearList(&list);
    assert(getSize(&list) == 0);
    populateList();
    clearList(&list);
    assert(getSize(&list) == 0);
}
END_TEST



START_TEST(test_arraylist_remove){
    for(int i = 0; i < getSize(&list); i++)
        free(removeFromList(&list, i));
    for(int i = 0; i < getSize(&list); i++)
        assert(*(int*)getElement(&list, i) == i * 2 + 1);
}
END_TEST
START_TEST(test_arraylist_pop){
    for(int i = size - 1; i >= 0; i--){
        void* p = pop(&list);
        assert(*(int*)p == i);
        free(p);
    }
    assert(!isNotEmpty(&list));
}
END_TEST

START_TEST(test_get_last){
    assert(*(int*)getLast(&list) == size - 1);
}
END_TEST

START_TEST(test_indexof){
    for(int i = 0; i < getSize(&list); i++)
        assert(indexOf(&list, &i, sizeof(int)) == i);
    for(int i = 0; i < getSize(&list); i++)
        assert(*(int*)find(&list, &i, sizeof(int)) == i);
    int outOfRange = size;
    assert(indexOf(&list, &outOfRange, sizeof(int)) == -1);
    assert(find(&list, &outOfRange, sizeof(int)) == NULL);
}
END_TEST

START_TEST(test_swap){
    int maxIndex = getSize(&list) - 1;
    for(int i = 0; i < getSize(&list); i++){
        swap(&list, i, maxIndex);
        assert(indexOf(&list, &i, sizeof(int)) == maxIndex);
        swap(&list, i, maxIndex);
        assert(indexOf(&list, &i, sizeof(int)) == i);
    }
}
END_TEST

START_TEST(test_get_next_index){
    assert(getNextIndex(&list, 0, -1) == getSize(&list) - 1);
    assert(getNextIndex(&list, 0, getSize(&list) - 1) == getSize(&list) - 1);
    assert(getNextIndex(&list, 0, 0) == 0);
    assert(getNextIndex(&list, 0, getSize(&list)) == 0);
    assert(getNextIndex(&list, 0, -getSize(&list)) == 0);
    for(int i = 0; i < getSize(&list) * 2; i++){
        assert(getNextIndex(&list, 0, i) == i % getSize(&list));
    }
}
END_TEST

START_TEST(test_shift_to_head){
    for(int i = 0; i < getSize(&list); i++)
        shiftToHead(&list, i);
    for(int i = 0; i < size; i++)
        assert(*(int*)getElement(&list, i) == size - i - 1);
}
END_TEST

START_TEST(test_get_time){
    unsigned int time = getTime();
    int delay = 11;
    for(int i = 0; i < 100; i++){
        msleep(delay);
        assert(time + delay <= getTime());
        time = getTime();
    }
}
END_TEST
START_TEST(test_min_max){
    int big = 10, small = 1;
    assert(MAX(big, small) == big);
    assert(MIN(big, small) == small);
}
END_TEST

Suite* utilSuite(){
    Suite* s = suite_create("Util");
    TCase* tc_core;
    tc_core = tcase_create("ArrayList");
    tcase_add_checked_fixture(tc_core, populateList, freeListMem);
    tcase_add_test(tc_core, test_arraylist);
    tcase_add_test(tc_core, test_indexof);
    tcase_add_test(tc_core, test_swap);
    tcase_add_test(tc_core, test_get_next_index);
    tcase_add_test(tc_core, test_arraylist_remove);
    tcase_add_test(tc_core, test_arraylist_pop);
    tcase_add_test(tc_core, test_get_last);
    tcase_add_test(tc_core, test_shift_to_head);
    suite_add_tcase(s, tc_core);
    tc_core = tcase_create("MISC");
    tcase_add_test(tc_core, test_get_time);
    tcase_add_test(tc_core, test_min_max);
    suite_add_tcase(s, tc_core);
    return s;
}
