#ifndef BITSET_H_
#define BITSET_H_

#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include "assert_helpers.h"
#include "threading.h"

/**
 * A simple synchronized bitset class.
 */
class SyncBitset {

public:
	/**
	 * Allocate enough words to accommodate 'sz' bits.  Output the given
	 * error message and quit if allocation fails.
	 */
	SyncBitset(size_t sz, const char *errmsg = NULL) : _errmsg(errmsg) {
		MUTEX_INIT(_lock);
		size_t nwords = (sz >> 5)+1; // divide by 32 and add 1
		try {
			_words = new uint32_t[nwords];
		} catch(std::bad_alloc& ba) {
			if(_errmsg != NULL) {
				std::cerr << _errmsg;
			}
			exit(1);
		}
		assert(_words != NULL);
		memset(_words, 0, nwords * 4 /* words to bytes */);
		_sz = nwords << 5 /* words to bits */;
	}

	/**
	 * Free memory for words.
	 */
	~SyncBitset() {
		delete[] _words;
	}

	/**
	 * Test whether the given bit is set in an unsynchronized manner.
	 */
	bool testUnsync(size_t i) {
		if(i < _sz) {
			return ((_words[i >> 5] >> (i & 0x1f)) & 1) != 0;
		}
		return false;
	}

	/**
	 * Test whether the given bit is set in a synchronized manner.
	 */
	bool test(size_t i) {
		bool ret = false;
		MUTEX_LOCK(_lock);
		if(i < _sz) {
			ret = ((_words[i >> 5] >> (i & 0x1f)) & 1) != 0;
		}
		MUTEX_UNLOCK(_lock);
		return ret;
	}

	/**
	 * Set a bit in the vector that hasn't been set before.  Assert if
	 * it has been set.  Uses synchronization.
	 */
	void set(size_t i) {
		MUTEX_LOCK(_lock);
		while(i >= _sz) {
			// Slow path: bitset needs to be expanded before the
			// specified bit can be set
			ASSERT_ONLY(size_t oldsz = _sz);
			expand();
			assert_gt(_sz, oldsz);
		}
		// Fast path
		assert_lt(i, _sz);
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 0);
		_words[i >> 5] |= (1 << (i & 0x1f));
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
		MUTEX_UNLOCK(_lock);
	}

	/**
	 * Set a bit in the vector that might have already been set.  Uses
	 * synchronization.
	 */
	void setOver(size_t i) {
		MUTEX_LOCK(_lock);
		while(i >= _sz) {
			// Slow path: bitset needs to be expanded before the
			// specified bit can be set
			ASSERT_ONLY(size_t oldsz = _sz);
			expand();
			assert_gt(_sz, oldsz);
		}
		// Fast path
		assert_lt(i, _sz);
		_words[i >> 5] |= (1 << (i & 0x1f));
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
		MUTEX_UNLOCK(_lock);
	}


private:

	/**
	 * Expand the size of the _words array by 50% to accommodate more
	 * bits.
	 */
	void expand() {
		size_t oldsz = _sz;
		_sz += (_sz >> 1);     // Add 50% more elements
		_sz += 7; _sz &= ~0x7; // Make sure it's 8-aligned
		uint32_t *newwords;
		try {
			newwords = new uint32_t[_sz >> 5 /* convert to words */];
		} catch(std::bad_alloc& ba) {
			if(_errmsg != NULL) {
				// Output given error message
				std::cerr << _errmsg;
			}
			exit(1);
		}
		// Move old values into new array
		memcpy(newwords, _words, oldsz >> 3 /* convert to bytes */);
		// Initialize all new words to 0
		memset(newwords + (oldsz >> 5 /*convert to words*/), 0,
		       (_sz - oldsz) >> 3 /* convert to bytes */);
		delete[] _words;   // delete old array
		_words = newwords; // install new array
	}

	const char *_errmsg; // error message if an allocation fails
	size_t _sz;          // size as # of bits
	MUTEX_T _lock;       // mutex
	uint32_t *_words;    // storage
};

/**
 * A simple unsynchronized bitset class.
 */
class Bitset {

public:
	Bitset(size_t sz, const char *errmsg = NULL) : _errmsg(errmsg) {
		size_t nwords = (sz >> 5)+1;
		try {
			_words = new uint32_t[nwords];
		} catch(std::bad_alloc& ba) {
			if(_errmsg != NULL) {
				std::cerr << _errmsg;
			}
			exit(1);
		}
		assert(_words != NULL);
		memset(_words, 0, nwords * 4);
		_sz = nwords << 5;
	}

	~Bitset() {
		delete[] _words;
	}

	/**
	 * Test whether the given bit is set.
	 */
	bool test(size_t i) {
		bool ret = false;
		if(i < _sz) {
			ret = ((_words[i >> 5] >> (i & 0x1f)) & 1) != 0;
		}
		return ret;
	}

	/**
	 * Set a bit in the vector that hasn't been set before.  Assert if
	 * it has been set.
	 */
	void set(size_t i) {
		while(i >= _sz) {
			// Slow path: bitset needs to be expanded before the
			// specified bit can be set
			ASSERT_ONLY(size_t oldsz = _sz);
			expand();
			assert_gt(_sz, oldsz);
		}
		// Fast path
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 0);
		_words[i >> 5] |= (1 << (i & 0x1f));
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
	}

	/**
	 * Set a bit in the vector that might have already been set.
	 */
	void setOver(size_t i) {
		while(i >= _sz) {
			// Slow path: bitset needs to be expanded before the
			// specified bit can be set
			ASSERT_ONLY(size_t oldsz = _sz);
			expand();
			assert_gt(_sz, oldsz);
		}
		// Fast path
		_words[i >> 5] |= (1 << (i & 0x1f));
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
	}

private:

	/**
	 * Expand the size of the _words array by 50% to accommodate more
	 * bits.
	 */
	void expand() {
		size_t oldsz = _sz;
		_sz += (_sz>>1); // Add 50% more elements
		uint32_t *newwords;
		try {
			newwords = new uint32_t[_sz >> 5];
		} catch(std::bad_alloc& ba) {
			if(_errmsg != NULL) {
				std::cerr << _errmsg;
			}
			exit(1);
		}
		memcpy(newwords, _words, oldsz >> 3);
		delete[] _words;
		memset(newwords + (oldsz >> 5), 0, (oldsz >> 4));
		_words = newwords;
	}

	const char *_errmsg; // error message if an allocation fails
	size_t _sz;          // size as # of bits
	uint32_t *_words;    // storage
};

/**
 * A simple fixed-length unsynchronized bitset class.
 */
template<int LEN>
class FixedBitset {

public:
	FixedBitset() : _cnt(0), _size(0) {
		memset(_words, 0, ((LEN>>5)+1) * 4);
	}

	/**
	 * Unset all bits.
	 */
	void clear() {
		memset(_words, 0, ((LEN>>5)+1) * 4);
	}

	/**
	 * Return true iff the bit at offset i has been set.
	 */
	bool test(size_t i) const {
		bool ret = false;
		assert_lt(i, LEN);
		ret = ((_words[i >> 5] >> (i & 0x1f)) & 1) != 0;
		return ret;
	}

	/**
	 * Set the bit at offset i.  Assert if the bit was already set.
	 */
	void set(size_t i) {
		// Fast path
		assert_lt(i, LEN);
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 0);
		_words[i >> 5] |= (1 << (i & 0x1f));
		_cnt++;
		if(i >= _size) {
			_size = i+1;
		}
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
	}

	/**
	 * Set the bit at offset i.  Do not assert if the bit was already
	 * set.
	 */
	void setOver(size_t i) {
		// Fast path
		assert_lt(i, LEN);
		_words[i >> 5] |= (1 << (i & 0x1f));
		_cnt++;
		if(i >= _size) {
			_size = i+1;
		}
		assert(((_words[i >> 5] >> (i & 0x1f)) & 1) == 1);
	}

	size_t count() const { return _cnt; }
	size_t size() const  { return _size; }

	/**
	 * Return true iff this FixedBitset has the same bits set as
	 * FixedBitset 'that'.
	 */
	bool operator== (const FixedBitset<LEN>& that) const {
		for(size_t i = 0; i < (LEN>>5)+1; i++) {
			if(_words[i] != that._words[i]) {
				return false;
			}
		}
		return true;
	}

	/**
	 * Return true iff this FixedBitset does not have the same bits set
	 * as FixedBitset 'that'.
	 */
	bool operator!= (const FixedBitset<LEN>& that) const {
		for(size_t i = 0; i < (LEN>>5)+1; i++) {
			if(_words[i] != that._words[i]) {
				return true;
			}
		}
		return false;
	}

	/**
	 * Return a string-ized version of this FixedBitset.
	 */
	std::string str() const {
		std::ostringstream oss;
		for(int i = (int)size()-1; i >= 0; i--) {
			oss << (test(i)? "1" : "0");
		}
		return oss.str();
	}

private:
	size_t _cnt;
	size_t _size;
	uint32_t _words[(LEN>>5)+1]; // storage
};

#endif /* BITSET_H_ */
