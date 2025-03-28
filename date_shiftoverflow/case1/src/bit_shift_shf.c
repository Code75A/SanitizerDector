/********Software Analysis - FY2013*************/
/*
* File Name: bit_shift.c
* Defect Classification
* ---------------------
* Defect Type: Numerical defects
* Defect Sub-type: Bit shift bigger than integral type or negative
* Description: Defect Code to identify bit shift errors
*/
#include "HeaderFile.h"
int rand (void);

/*
  * Types of defects: bit shift error
  * Complexity: constant shift to the left beyond the int size
  */
int bit_shift_SaniTestArr0[32];
int bit_shift_SaniCatcher0;
void bit_shift_001 ()
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher0 = bit_shift_SaniTestArr0[32]),a << 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: long	Beyond the size of the left shift - Constant
 */
int bit_shift_SaniTestArr1[64];
int bit_shift_SaniCatcher1;
void bit_shift_002 ()
{
	long a = 1;
	long ret;
	ret = (bit_shift_SaniCatcher1 = bit_shift_SaniTestArr1[32]),a << 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: unsigned int	Beyond the size of the left shift -Constant
 */
int bit_shift_SaniTestArr2[32];
int bit_shift_SaniCatcher2;
void bit_shift_003 ()
{
	unsigned int a = 1;
	unsigned int ret;
	ret = (bit_shift_SaniCatcher2 = bit_shift_SaniTestArr2[32]),a << 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: unsigned long	Beyond the size of the left shift	Constant
 */
int bit_shift_SaniTestArr3[64];
int bit_shift_SaniCatcher3;
void bit_shift_004 ()
{
	unsigned long a = 1;
	unsigned long ret;
	ret = (bit_shift_SaniCatcher3 = bit_shift_SaniTestArr3[32]),a << 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Shift left on negative values	Constant
 */
int bit_shift_SaniTestArr4[32];
int bit_shift_SaniCatcher4;
void bit_shift_005 ()
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher4 = bit_shift_SaniTestArr4[-1]),a << -1;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the right shift	Constant
 */
int bit_shift_SaniTestArr5[32];
int bit_shift_SaniCatcher5;
void bit_shift_006 ()
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher5 = bit_shift_SaniTestArr5[32]),a >> 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Right shifting negative values with	Constant
 */
int bit_shift_SaniTestArr6[32];
int bit_shift_SaniCatcher6;
void bit_shift_007 ()
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher6 = bit_shift_SaniTestArr6[-1]),a >> -1;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Variable
 */
int bit_shift_SaniTestArr7[32];
int bit_shift_SaniCatcher7;
void bit_shift_008 ()
{
	int a = 1;
	int shift = 32;
	int ret;
	ret = (bit_shift_SaniCatcher7 = bit_shift_SaniTestArr7[shift]),a << shift;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Value of random variable
 */
int bit_shift_SaniTestArr8[32];
int bit_shift_SaniCatcher8;
void bit_shift_009 ()
{
	int a = 1;
	int shift;
	int ret;
	shift = rand();
	ret = (bit_shift_SaniCatcher8 = bit_shift_SaniTestArr8[shift]),a << shift;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Linear equation
 */
int bit_shift_SaniTestArr9[32];
int bit_shift_SaniCatcher9;
void bit_shift_010 ()
{
	int a = 1;
	int shift = 6;
	int ret;
	ret = (bit_shift_SaniCatcher9 = bit_shift_SaniTestArr9[((5 * shift) + 2)]),a << ((5 * shift) + 2);/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Nonlinear equation
 */
int bit_shift_SaniTestArr10[32];
int bit_shift_SaniCatcher10;
void bit_shift_011 ()
{
	int a = 1;
	int shift = 5;
	int ret;
	ret = (bit_shift_SaniCatcher10 = bit_shift_SaniTestArr10[((shift * shift) + 7)]),a << ((shift * shift) + 7);/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	The return value of the function
 */
int bit_shift_012_func_001 ()
{
	return 32;
}

int bit_shift_SaniTestArr11[32];
int bit_shift_SaniCatcher11;
void bit_shift_012 ()
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher11 = bit_shift_SaniTestArr11[bit_shift_012_func_001()]),a << bit_shift_012_func_001();/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Function arguments
 */
int bit_shift_SaniTestArr12[32];
int bit_shift_SaniCatcher12;
void bit_shift_013_func_001 (int shift)
{
	int a = 1;
	int ret;
	ret = (bit_shift_SaniCatcher12 = bit_shift_SaniTestArr12[shift]),a << shift;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

void bit_shift_013 ()
{
	bit_shift_013_func_001(32);
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	An array of element values
 */
int bit_shift_SaniTestArr13[32];
int bit_shift_SaniCatcher13;
void bit_shift_014 ()
{
	int a = 1;
	int shifts[5] = {8, 40, 16, 32, 24};
	int ret;
	ret = (bit_shift_SaniCatcher13 = bit_shift_SaniTestArr13[shifts[3]]),a << shifts[3];/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Alias for 1 weight
 */
int bit_shift_SaniTestArr14[32];
int bit_shift_SaniCatcher14;
void bit_shift_015 ()
{
	int a = 1;
	int shift = 32;
	int shift1;
	int ret;
	shift1 = shift;
	ret = (bit_shift_SaniCatcher14 = bit_shift_SaniTestArr14[shift1]),a << shift1;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: int	Beyond the size of the left shift	Also known as double
 */
int bit_shift_SaniTestArr15[32];
int bit_shift_SaniCatcher15;
void bit_shift_016 ()
{
	int a = 1;
	int shift = 32;
	int shift1;
	int shift2;
	int ret;
	shift1 = shift;
	shift2 = shift1;
	ret = (bit_shift_SaniCatcher15 = bit_shift_SaniTestArr15[shift2]),a << shift2;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: the operands is a constant
 */
int bit_shift_SaniTestArr16[32];
int bit_shift_SaniCatcher16;
void bit_shift_017 ()
{
	int ret;
	ret = (bit_shift_SaniCatcher16 = bit_shift_SaniTestArr16[32]),1 << 32;/*Tool should detect this line as error*/ /*ERROR:Bit shift error*/
        sink = ret;
}

/*
 * Types of defects: BitShift errors
 * Complexity: BitShift errors Main function
 */
extern volatile int vflag;
void bit_shift_main ()
{
	if (vflag == 1 || vflag ==888)
	{
		bit_shift_001();
	}

	if (vflag == 2 || vflag ==888)
	{
		bit_shift_002();
	}

	if (vflag == 3 || vflag ==888)
	{
		bit_shift_003();
	}

	if (vflag == 4 || vflag ==888)
	{
		bit_shift_004();
	}

	if (vflag == 5 || vflag ==888)
	{
		bit_shift_005();
	}

	if (vflag == 6 || vflag ==888)
	{
		bit_shift_006();
	}

	if (vflag == 7 || vflag ==888)
	{
		bit_shift_007();
	}

	if (vflag == 8 || vflag ==888)
	{
		bit_shift_008();
	}

	if (vflag == 9 || vflag ==888)
	{
		bit_shift_009();
	}

	if (vflag == 10 || vflag ==888)
	{
		bit_shift_010();
	}

	if (vflag == 11 || vflag ==888)
	{
		bit_shift_011();
	}

	if (vflag == 12 || vflag ==888)
	{
		bit_shift_012();
	}

	if (vflag == 13 || vflag ==888)
	{
		bit_shift_013();
	}

	if (vflag == 14 || vflag ==888)
	{
		bit_shift_014();
	}

	if (vflag == 15 || vflag ==888)
	{
		bit_shift_015();
	}

	if (vflag == 16 || vflag ==888)
	{
		bit_shift_016();
	}

	if (vflag == 17 || vflag ==888)
	{
		bit_shift_017();
	}
}
