/* NUMarrays.cpp
 *
 * Copyright (C) 1992-2012,2017 Paul Boersma
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

#include "melder.h"

static integer theTotalNumberOfArrays;

integer NUM_getTotalNumberOfArrays () { return theTotalNumberOfArrays; }

#pragma mark - Generic memory functions for vectors

char * NUMvector_generic (integer elementSize, integer lo, integer hi, bool initializeToZero) {
	try {
		if (hi < lo) return nullptr;   // not an error
		char *result;
		Melder_assert (sizeof (char) == 1);   // some say that this is true by definition
		for (;;) {   // not very infinite: 99.999 % of the time once, 0.001 % twice
			result = initializeToZero ?
				reinterpret_cast<char*> (_Melder_calloc (hi - lo + 1, elementSize)) :
				reinterpret_cast<char*> (_Melder_malloc ((hi - lo + 1) * elementSize));
			if (result -= lo * elementSize) break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (result + lo * elementSize, 1);   // make "sure" that the second try will succeed (not *very* sure, because realloc might move memory even if it shrinks)
		}
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Vector of elements not created.");
	}
}

void NUMvector_free_generic (integer elementSize, char *vector, integer lo) noexcept {
	if (! vector) return;   // no error
	char *cells = & vector [lo * elementSize];
	Melder_free (cells);
	theTotalNumberOfArrays -= 1;
}

char * NUMvector_copy_generic (integer elementSize, char *vector, integer lo, integer hi) {
	try {
		if (! vector) return nullptr;
		char *result = NUMvector_generic (elementSize, lo, hi, false);
		char *p_cells = & vector [lo * elementSize];
		char *p_resultCells = & result [lo * elementSize];
		integer numberOfBytesToCopy = (hi - lo + 1) * elementSize;
		memcpy (p_resultCells, p_cells, (size_t) numberOfBytesToCopy);
		return result;
	} catch (MelderError) {
		Melder_throw (U"Vector of elements not copied.");
	}
}

void NUMvector_copyElements_generic (integer elementSize, char *fromVector, char *toVector, integer lo, integer hi) {
	Melder_assert (fromVector && toVector);
	char *p_fromCells = & fromVector [lo * elementSize];
	char *p_toCells   = & toVector   [lo * elementSize];
	integer numberOfBytesToCopy = (hi - lo + 1) * elementSize;
	if (hi >= lo) memcpy (p_toCells, p_fromCells, (size_t) numberOfBytesToCopy);   // BUG this assumes contiguity
}

bool NUMvector_equal_generic (integer elementSize, char *vector1, char *vector2, integer lo, integer hi) {
	Melder_assert (vector1 && vector2);
	char *p_cells1 = & vector1 [lo * elementSize];
	char *p_cells2 = & vector2 [lo * elementSize];
	integer numberOfBytesToCompare = (hi - lo + 1) * elementSize;
	return memcmp (p_cells1, p_cells2, (size_t) numberOfBytesToCompare) == 0;
}

void NUMvector_append_generic (integer elementSize, char **v, integer lo, integer *hi) {
	try {
		char *result;
		if (! *v) {
			result = NUMvector_generic (elementSize, lo, lo, true);
			*hi = lo;
		} else {
			integer offset = lo * elementSize;
			for (;;) {   // not very infinite: 99.999 % of the time once, 0.001 % twice
				result = reinterpret_cast <char *> (Melder_realloc ((char *) *v + offset, (*hi - lo + 2) * elementSize));
				if ((result -= offset) != nullptr) break;   // this will normally succeed at the first try
				(void) Melder_realloc_f (result + offset, 1);   // make "sure" that the second try will succeed
			}
			(*hi) ++;
			memset (result + *hi * elementSize, 0, elementSize);   // initialize the new element to zeroes
		}
		*v = result;
	} catch (MelderError) {
		Melder_throw (U"Vector: element not appended.");
	}
}

void NUMvector_insert_generic (integer elementSize, char **v, integer lo, integer *hi, integer position) {
	try {
		char *result;
		if (! *v) {
			result = NUMvector_generic (elementSize, lo, lo, true);
			*hi = lo;
			Melder_assert (position == lo);
		} else {
			result = NUMvector_generic (elementSize, lo, *hi + 1, false);
			Melder_assert (position >= lo && position <= *hi + 1);
			NUMvector_copyElements_generic (elementSize, *v, result, lo, position - 1);
			memset (result + position * elementSize, 0, elementSize);
			NUMvector_copyElements_generic (elementSize, *v, result + elementSize, position, *hi);
			NUMvector_free_generic (elementSize, *v, lo);
			(*hi) ++;
		}
		*v = result;
	} catch (MelderError) {
		Melder_throw (U"Vector: element not inserted.");
	}
}

/*** Generic memory functions for matrices. ***/

void * NUMmatrix (integer elementSize, integer row1, integer row2, integer col1, integer col2, bool initializeToZero) {
	try {
		int64 numberOfRows = row2 - row1 + 1;
		int64 numberOfColumns = col2 - col1 + 1;
		int64 numberOfCells = numberOfRows * numberOfColumns;

		/*
		 * Allocate room for the row pointers.
		 */
		char **result;
		Melder_assert (sizeof (char) == 1);   // true by definition
		for (;;) {
			result = reinterpret_cast <char **> (_Melder_malloc_f (numberOfRows * (int64) sizeof (char *)));   // assume that all pointers have the same size
			result -= row1;
			if (result) break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (result + row1, 1);   // make "sure" that the second try will succeed
		}
		/*
		 * Allocate room for the cells.
		 * The first row pointer points to below the first cell.
		 */
		for (;;) {
			try {
				result [row1] = initializeToZero ?
					reinterpret_cast <char *> (_Melder_calloc (numberOfCells, elementSize)) :
					reinterpret_cast <char *> (_Melder_malloc (numberOfCells * elementSize));
			} catch (MelderError) {
				result += row1;
				Melder_free (result);   // free the row pointers
				throw MelderError ();
			}
			if ((result [row1] -= col1 * elementSize) != nullptr) break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (result [row1] + col1 * elementSize, 1);   // make "sure" that the second try will succeed
		}
		int64 rowSize = numberOfColumns * elementSize;
		for (integer irow = row1 + 1; irow <= row2; irow ++) result [irow] = result [irow - 1] + rowSize;
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Matrix of elements not created.");
	}
}

char*** NUMtensor3_generic (integer elementSize, integer pla1, integer pla2, integer row1, integer row2, integer col1, integer col2, bool initializeToZero) {
	try {
		int64 numberOfPlanes = pla2 - pla1 + 1;
		int64 numberOfRows = row2 - row1 + 1;
		int64 numberOfColumns = col2 - col1 + 1;
		int64 numberOfCells = numberOfPlanes * numberOfRows * numberOfColumns;

		/*
			Allocate room for the plane pointers.
		*/
		char ***result;
		for (;;) {
			result = reinterpret_cast <char ***> (_Melder_malloc_f (numberOfPlanes * (int64) sizeof (char **)));   // assume that all pointers have the same size
			result -= pla1;
			if (result) break;   // this will normally succeed at the first try
			(void) Melder_realloc_f (result + pla1, 1);   // make "sure" that the second try will succeed
		}
		/*
			Allocate room for the row pointers.
		*/
		char **rowPointers =
			reinterpret_cast <char **> (_Melder_malloc_f (numberOfPlanes * numberOfRows * (int64) sizeof (char *)));   // assume that all pointers have the same size
		result [pla1] = & rowPointers [0];
		for (integer ipla = pla1 + 1; ipla <= pla2; ipla ++) {
			result [ipla] = result [ipla - 1] + numberOfRows;
		}
		/*
			Allocate room for the cells.
		*/
		char *cells = initializeToZero ?
			reinterpret_cast <char *> (_Melder_calloc (numberOfCells, elementSize)) :
			reinterpret_cast <char *> (_Melder_malloc (numberOfCells * elementSize));
		char **q = & rowPointers [0];
		char *p = & cells [0];
		int64 rowSize = numberOfColumns * elementSize;
		for (integer ipla = pla1; ipla <= pla2; ipla ++) {
			for (integer irow = row1; irow <= row2; irow ++) {
				*q = p;
				++ q;
				p += rowSize;
			}
		}
		theTotalNumberOfArrays += 1;
		return result;
	} catch (MelderError) {
		Melder_throw (U"Three-rank tensor of elements not created.");
	}
}

void NUMmatrix_free_ (integer elementSize, char **m, integer row1, integer col1) noexcept {
	if (! m) return;
	char *cells = & m [row1] [col1 * elementSize];
	Melder_free (cells);
	char **rowPointers = & m [row1];
	Melder_free (rowPointers);
	theTotalNumberOfArrays -= 1;
}

void NUMtensor3_free_generic (integer elementSize, char ***t, integer pla1, integer row1, integer col1) noexcept {
	if (! t) return;
	char *cells = & t [pla1] [row1] [col1 * elementSize];
	Melder_free (cells);
	char **rowPointers = & t [pla1] [row1];
	Melder_free (rowPointers);
	char ***planePointers = & t [pla1];
	Melder_free (planePointers);
	theTotalNumberOfArrays -= 1;
}

void * NUMmatrix_copy (integer elementSize, void *m, integer row1, integer row2, integer col1, integer col2) {
	try {
		if (! m) return nullptr;
		char **result = reinterpret_cast <char **> (NUMmatrix (elementSize, row1, row2, col1, col2, false));
		if (! result) return nullptr;
		integer columnOffset = col1 * elementSize;
		integer dataSize = (row2 - row1 + 1) * (col2 - col1 + 1) * elementSize;
		memcpy (result [row1] + columnOffset, ((char **) m) [row1] + columnOffset, (size_t) dataSize);
		return result;
	} catch (MelderError) {
		Melder_throw (U"Matrix of elements not copied.");
	}
}

void NUMmatrix_copyElements_ (integer elementSize, char **mfrom, char **mto, integer row1, integer row2, integer col1, integer col2) {
	Melder_assert (mfrom && mto);
	integer columnOffset = col1 * elementSize;
	integer dataSize = (row2 - row1 + 1) * (col2 - col1 + 1) * elementSize;
	memcpy (mto [row1] + columnOffset, mfrom [row1] + columnOffset, (size_t) dataSize);
}

bool NUMmatrix_equal (integer elementSize, void *m1, void *m2, integer row1, integer row2, integer col1, integer col2) {
	Melder_assert (m1 && m2);
	integer columnOffset = col1 * elementSize;
	integer dataSize = (row2 - row1 + 1) * (col2 - col1 + 1) * elementSize;
	return ! memcmp (((char **) m1) [row1] + columnOffset, ((char **) m2) [row1] + columnOffset, dataSize);
}

/*** Typed I/O routines for vectors and matrices. ***/

#define FUNCTION(type,storage)  \
	void NUMvector_writeText_##storage (const type *v, integer lo, integer hi, MelderFile file, const char32 *name) { \
		texputintro (file, name, U" []: ", hi >= lo ? nullptr : U"(empty)", 0,0,0); \
		for (integer i = lo; i <= hi; i ++) \
			texput##storage (file, v [i], name, U" [", Melder_integer (i), U"]", 0,0); \
		texexdent (file); \
		if (feof (file -> filePointer) || ferror (file -> filePointer)) Melder_throw (U"Write error."); \
	} \
	void NUMvector_writeBinary_##storage (const type *v, integer lo, integer hi, FILE *f) { \
		for (integer i = lo; i <= hi; i ++) \
			binput##storage (v [i], f); \
		if (feof (f) || ferror (f)) Melder_throw (U"Write error."); \
	} \
	type * NUMvector_readText_##storage (integer lo, integer hi, MelderReadText text, const char *name) { \
		type *result = nullptr; \
		try { \
			result = NUMvector <type> (lo, hi); \
			for (integer i = lo; i <= hi; i ++) { \
				try { \
					result [i] = texget##storage (text); \
				} catch (MelderError) { \
					Melder_throw (U"Could not read ", Melder_peek8to32 (name), U" [", i, U"]."); \
				} \
			} \
			return result; \
		} catch (MelderError) { \
			NUMvector_free (result, lo); \
			throw; \
		} \
	} \
	type * NUMvector_readBinary_##storage (integer lo, integer hi, FILE *f) { \
		type *result = nullptr; \
		try { \
			result = NUMvector <type> (lo, hi); \
			for (integer i = lo; i <= hi; i ++) { \
				result [i] = binget##storage (f); \
			} \
			return result; \
		} catch (MelderError) { \
			NUMvector_free (result, lo); \
			throw; \
		} \
	} \
	void NUMmatrix_writeText_##storage (type **m, integer row1, integer row2, integer col1, integer col2, MelderFile file, const char32 *name) { \
		texputintro (file, name, U" [] []: ", row2 >= row1 ? nullptr : U"(empty)", 0,0,0); \
		if (row2 >= row1) { \
			for (integer irow = row1; irow <= row2; irow ++) { \
				texputintro (file, name, U" [", Melder_integer (irow), U"]:", 0,0); \
				for (integer icol = col1; icol <= col2; icol ++) { \
					texput##storage (file, m [irow] [icol], name, U" [", Melder_integer (irow), U"] [", Melder_integer (icol), U"]"); \
				} \
				texexdent (file); \
			} \
		} \
		texexdent (file); \
		if (feof (file -> filePointer) || ferror (file -> filePointer)) Melder_throw (U"Write error."); \
	} \
	void NUMmatrix_writeBinary_##storage (type **m, integer row1, integer row2, integer col1, integer col2, FILE *f) { \
		if (row2 >= row1) { \
			for (integer irow = row1; irow <= row2; irow ++) { \
				for (integer icol = col1; icol <= col2; icol ++) \
					binput##storage (m [irow] [icol], f); \
			} \
		} \
		if (feof (f) || ferror (f)) Melder_throw (U"Write error."); \
	} \
	type ** NUMmatrix_readText_##storage (integer row1, integer row2, integer col1, integer col2, MelderReadText text, const char *name) { \
		type **result = nullptr; \
		try { \
			result = NUMmatrix <type> (row1, row2, col1, col2); \
			for (integer irow = row1; irow <= row2; irow ++) for (integer icol = col1; icol <= col2; icol ++) { \
				try { \
					result [irow] [icol] = texget##storage (text); \
				} catch (MelderError) { \
					Melder_throw (U"Could not read ", Melder_peek8to32 (name), U" [", irow, U"] [", icol, U"]."); \
				} \
			} \
			return result; \
		} catch (MelderError) { \
			NUMmatrix_free (result, row1, col1); \
			throw; \
		} \
	} \
	type ** NUMmatrix_readBinary_##storage (integer row1, integer row2, integer col1, integer col2, FILE *f) { \
		type **result = nullptr; \
		try { \
			result = NUMmatrix <type> (row1, row2, col1, col2); \
			for (integer irow = row1; irow <= row2; irow ++) for (integer icol = col1; icol <= col2; icol ++) \
				result [irow] [icol] = binget##storage (f); \
			return result; \
		} catch (MelderError) { \
			NUMmatrix_free (result, row1, col1); \
			throw; \
		} \
	}

FUNCTION (signed char, i8)
FUNCTION (int, i16)
FUNCTION (long, i32)
FUNCTION (integer, integer)
FUNCTION (unsigned char, u8)
FUNCTION (unsigned int, u16)
FUNCTION (unsigned long, u32)
FUNCTION (double, r32)
FUNCTION (double, r64)
FUNCTION (dcomplex, c64)
FUNCTION (dcomplex, c128)
#undef FUNCTION

/* End of file NUMarrays.cpp */
