/*!
 * @file main.c
 * @brief SofaBuffers C - Test
 *
 * SPDX-License-Identifier: MIT
 */

#include "unity.h"
#include "sofab/object.h"

/*****************************************************************************/
/* unity hooks */
/*****************************************************************************/

void setUp(void)
{
    // This is run before EACH TEST
}

void tearDown(void)
{
    // This is run after EACH TEST
}

/*****************************************************************************/
/* tests */
/*****************************************************************************/

int test_ostream_main (void);
int test_istream_main (void);
int test_object_main (void);

int main (void)
{
    int result = 0;

    result |= test_ostream_main();
    result |= test_istream_main();
    result |= test_object_main();

    return result;
}
