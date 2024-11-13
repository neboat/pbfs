// -*- C++ -*-
// Copyright (c) 2010-2024, Tao B. Schardl
/*
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef BAG_H
#define BAG_H

#include <cassert>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <cstdlib>
#include <cstring>

#define FILLING_ARRAY true

// Macros for reducer testing
#define REDUCER_ORIG                                                           \
  1 // Use the old C++ reducer syntax (no longer works in OpenCilk 2)
#define REDUCER_ARRAY                                                          \
  2 // New reducer syntax, creates an array of 2 bag reducers (segfaults)
#define REDUCER_PTRS                                                           \
  3 // New reducer syntax, individually creates 2 bag reducer variables (works)

#ifndef REDUCER_IMPL
#define REDUCER_IMPL REDUCER_PTRS
#endif

const uint32_t BAG_SIZE = 64;
const uint32_t BLK_SIZE = 2048;

template <typename T> class Bag;
template <typename T> class Bag_reducer;

template <typename T> class Pennant {
private:
  T *els;
  Pennant<T> *l, *r;

public:
  Pennant();
  Pennant(T *);
  ~Pennant();

  inline const T *getElements();
  inline Pennant<T> *getLeft();
  inline Pennant<T> *getRight();

  inline void clearChildren() {
    l = NULL;
    r = NULL;
  }

  inline Pennant<T> *combine(Pennant<T> *);
  inline Pennant<T> *split();

  friend class Bag<T>;
  friend class Bag_reducer<T>;
};

template <typename T> class Bag {
  // private:
public: // HACKS
  // fill points to the bag entry one position beyond the MSP
  uint32_t fill;
  Pennant<T> **bag;

#if FILLING_ARRAY
  T *filling;
#else
  Pennant<T> *filling;
#endif // FILLING_ARRAY

  // size points to the filling array entry on position
  // beyond last valid element.
  uint32_t size;

  inline void insert_h();
  inline void insert_fblk(T *fblk);
  inline void insert_blk(T *blk, uint32_t size);

public:
  Bag();
  Bag(Bag<T> *);

  ~Bag();

  inline void insert(T);
  void merge(Bag<T> *);
  inline bool split(Pennant<T> **);
  int split(Pennant<T> **, int);

  inline uint32_t numElements() const;
  inline uint32_t getFill() const;
  inline bool isEmpty() const;
  inline Pennant<T> *getFirst() const;
  inline T *getFilling() const;
  inline uint32_t getFillingSize() const;

  void clear();

  static void identity(void *view) { new (view) Bag; }
  static void reduce(void *left, void *right) {
    static_cast<Bag *>(left)->merge(static_cast<Bag *>(right));
    static_cast<Bag *>(right)->~Bag();
  }

#if REDUCER_IMPL == REDUCER_ORIG
  friend class Bag_reducer<T>;
  friend class cilk::monoid_base<Bag<T>>;
#endif
};

#if REDUCER_IMPL != REDUCER_ORIG

template <typename T>
using Bag_red = Bag<T> cilk_reducer(Bag<T>::identity, Bag<T>::reduce);

#else // REDUCER_IMPL

template <typename T> class Bag_reducer {
public:
  struct Monoid : cilk::monoid_base<Bag<T>> {
    static void reduce(Bag<T> *left, Bag<T> *right) { left->merge(right); }
  };

private:
  cilk::reducer<Monoid> imp_;

public:
  Bag_reducer();

  void insert(T);
  void merge(Bag_reducer<T> *);
  inline bool split(Pennant<T> **);
  int split(Pennant<T> **, int);

  inline Bag<T> &get_reference();

  inline uint32_t numElements() const;
  inline uint32_t getFill() const;
  inline bool isEmpty() const;
  inline Pennant<T> *getFirst() const;
  inline T *getFilling() const;
  inline uint32_t getFillingSize() const;

  inline void clear();
};

template <typename T> Bag_reducer<T>::Bag_reducer() : imp_() {}

template <typename T> void Bag_reducer<T>::insert(T el) {
  imp_.view().insert(el);
}

template <typename T> void Bag_reducer<T>::merge(Bag_reducer<T> *that) {
  this->imp_.view().merge(that->imp_.view());
}

template <typename T> inline bool Bag_reducer<T>::split(Pennant<T> **p) {
  return imp_.view().split(p);
}

template <typename T> int Bag_reducer<T>::split(Pennant<T> **p, int pos) {
  return imp_.view().split(p, pos);
}

template <typename T> inline Bag<T> &Bag_reducer<T>::get_reference() {
  return imp_.view();
}

template <typename T> inline uint32_t Bag_reducer<T>::numElements() const {
  return imp_.view().numElements();
}

template <typename T> inline uint32_t Bag_reducer<T>::getFill() const {
  return imp_.view().getFill();
}

template <typename T> inline bool Bag_reducer<T>::isEmpty() const {
  return imp_.view().isEmpty();
}

template <typename T> inline Pennant<T> *Bag_reducer<T>::getFirst() const {
  return imp_.view().getFirst();
}

template <typename T> inline T *Bag_reducer<T>::getFilling() const {
  return imp_.view().getFilling();
}

template <typename T> inline uint32_t Bag_reducer<T>::getFillingSize() const {
  return imp_.view().getFillingSize();
}

template <typename T> inline void Bag_reducer<T>::clear() {
  imp_.view().clear();
}

#endif // REDUCER_IMPL

template <typename T> T MAX(T a, T b) { return (a > b) ? a : b; }

//////////////////////////////////
///                            ///
/// Pennant method definitions ///
///                            ///
//////////////////////////////////
template <typename T> Pennant<T>::Pennant() {
  this->els = new T[BLK_SIZE];
  this->l = NULL;
  this->r = NULL;
}

// els_array must have size BLK_SIZE
template <typename T> Pennant<T>::Pennant(T els_array[]) {
  this->els = els_array;
  this->l = NULL;
  this->r = NULL;
}

template <typename T> Pennant<T>::~Pennant() { delete[] els; }

template <typename T> inline const T *Pennant<T>::getElements() {
  return this->els;
}

template <typename T> inline Pennant<T> *Pennant<T>::getLeft() { return l; }

template <typename T> inline Pennant<T> *Pennant<T>::getRight() { return r; }

/*
 * This method assumes that the pennant rooted at <that> has the
 * same number of nodes as the pennant rooted at <this>.
 */
template <typename T> inline Pennant<T> *Pennant<T>::combine(Pennant<T> *that) {
  that->r = this->l;
  this->l = that;

  return this;
}

/*
 * This method performs the inverse operation of
 * Pennant<T>::combine and places half of the split here and the
 * the other half at the returned pointer
 */
template <typename T> inline Pennant<T> *Pennant<T>::split() {
  Pennant<T> *that;

  that = this->l;
  this->l = that->r;
  that->r = NULL;

  return that;
}

//////////////////////////////
///                        ///
/// Bag method definitions ///
///                        ///
//////////////////////////////
template <typename T> Bag<T>::Bag() : fill(0), size(0) {
  this->bag = new Pennant<T> *[BAG_SIZE];
#if FILLING_ARRAY
  this->filling = new T[BLK_SIZE];
#else
  this->filling = new Pennant<T>();
#endif
}

/*
 * Copy Constructor. Performs a shallow bag copy.
 * Useful for verifying Bag implementation correctness, since
 * the split method destroys the Bag.
 */
template <typename T>
Bag<T>::Bag(Bag<T> *that) : fill(that->fill), size(that->size) {
  this->bag = new Pennant<T> *[BAG_SIZE];
  for (uint32_t i = 0; i < BAG_SIZE; i++)
    this->bag[i] = that->bag[i];

  this->filling = that->filling;
}

template <typename T> Bag<T>::~Bag() {
  if (this->bag != NULL) {
    for (int i = 0; i < this->fill; i++) {
      if (this->bag[i] != NULL) {
        delete this->bag[i];
        this->bag[i] = NULL;
      }
    }

    delete[] this->bag;
  }
#if FILLING_ARRAY
  delete[] this->filling;
#else
  delete this->filling;
#endif // FILLING_ARRAY
}

template <typename T> inline uint32_t Bag<T>::numElements() const {
  uint32_t count = this->size;
  uint32_t k = 1;
  for (uint32_t i = 0; i < this->fill; i++) {
    if (this->bag[i] != NULL)
      count += k * BLK_SIZE;
    k = k * 2;
  }

  return count;
}

// helper routine to perform bag-insert with filled filling array
template <typename T> inline void Bag<T>::insert_h() {
#if FILLING_ARRAY
  Pennant<T> *c = new Pennant<T>(this->filling);
  this->filling = new T[BLK_SIZE];
#else
  Pennant<T> *c = this->filling;
  this->filling = new Pennant<T>();
#endif // FILLING_ARRAY
  // this->filling->els[0] = el;
  // this->size = 1;
  this->size = 0;

  uint32_t i = 0;
  do {

    if (i < this->fill && this->bag[i] != NULL) {

      c = this->bag[i]->combine(c);
      this->bag[i] = NULL;

    } else {

      this->bag[i] = c;
      // this->fill = this->fill > i+1 ? this->fill : i+1;
      this->fill = MAX(this->fill, i + 1);
      return;
    }
    // switch(i < this->fill && this->bag[i] != NULL) {
    // case false:
    //   this->bag[i] = c;
    //   this->fill = this->fill > i+1 ? this->fill : i+1;
    //   return;
    // case true:
    //   c = this->bag[i]->combine(c);
    //   this->bag[i] = NULL;
    //   break;
    // default: break;
    // }

    ++i;

  } while (i < BAG_SIZE);

  this->fill = BAG_SIZE;
}

// helper routine to perform bag-insert with filled filling array
template <typename T> inline void Bag<T>::insert_fblk(T fblk[]) {
  Pennant<T> *c = new Pennant<T>(fblk);

  uint32_t i = 0;
  do {

    if (i < this->fill && this->bag[i] != NULL) {

      c = this->bag[i]->combine(c);
      this->bag[i] = NULL;

    } else {

      this->bag[i] = c;
      // this->fill = this->fill > i+1 ? this->fill : i+1;
      this->fill = MAX(this->fill, i + 1);
      return;
    }
    // switch(i < this->fill && this->bag[i] != NULL) {
    // case false:
    //   this->bag[i] = c;
    //   this->fill = this->fill > i+1 ? this->fill : i+1;
    //   return;
    // case true:
    //   c = this->bag[i]->combine(c);
    //   this->bag[i] = NULL;
    //   break;
    // default: break;
    // }

    ++i;

  } while (i < BAG_SIZE);

  this->fill = BAG_SIZE;
}

template <typename T> inline void Bag<T>::insert_blk(T blk[], uint32_t size) {
  int i;

  // Deal with the partially-filled Pennants
  if (this->size < size) {
    // Copy contents of this->filling into blk
    i = this->size - (BLK_SIZE - size);

    if (i >= 0) {
      // Contents of this->filling fill blk
#if FILLING_ARRAY
      memcpy(blk + size, this->filling + i, (BLK_SIZE - size) * sizeof(T));
#else
      memcpy(blk + size, this->filling->els + i, (BLK_SIZE - size) * sizeof(T));
#endif // FILLING_ARRAY

      // carry = blk;
      insert_fblk(blk);

      this->size = i;
    } else {
      // Contents of this->filling do not fill blk
      // if (this->size > 0) {
#if FILLING_ARRAY
      memcpy(blk + size, this->filling, this->size * sizeof(T));
#else
      memcpy(blk + size, this->filling->els, this->size * sizeof(T));
#endif // FILLING_ARRAY

      delete[] this->filling;

#if FILLING_ARRAY
      this->filling = blk;
#else
      this->filling->els = blk;
#endif // FILLING_ARRAY
      this->size += size;
    }

  } else {
    // Copy contents of blk into this->filling
    T *carry;
    i = size - (BLK_SIZE - this->size);

    if (i >= 0) {
      // Contents of blk fill this->filling
#if FILLING_ARRAY
      memcpy(this->filling + this->size, blk + i,
             (BLK_SIZE - this->size) * sizeof(T));

      carry = this->filling;
      this->filling = blk;

#else
      memcpy(this->filling->els + this->size, blk + i,
             (BLK_SIZE - this->size) * sizeof(T));

      carry = this->filling->els;
      this->filling->els = blk;

#endif // FILLING_ARRAY

      this->size = i;
      insert_fblk(carry);

    } else {
      // Contents of blk do not fill this->filling
      // if (that->size > 0) {
#if FILLING_ARRAY
      memcpy(this->filling + this->size, blk, size * sizeof(T));
#else
      memcpy(this->filling->els + this->size, blk, size * sizeof(T));
#endif // FILLING_ARRAY
       //}
      this->size += size;
    }
  }
}

template <typename T> inline void Bag<T>::insert(T el) {
  // assert(this->size < BLK_SIZE);
#if FILLING_ARRAY
  this->filling[this->size++] = el;
#else
  this->filling->els[this->size++] = el;
#endif // FILLING_ARRAY

  if (this->size < BLK_SIZE) {
    return;
  }

#if FILLING_ARRAY
  Pennant<T> *c = new Pennant<T>(this->filling);
  this->filling = new T[BLK_SIZE];
#else
  Pennant<T> *c = this->filling;
  this->filling = new Pennant<T>();
#endif // FILLING_ARRAY
  // this->filling->els[0] = el;
  // this->size = 1;
  this->size = 0;

  uint32_t i = 0;
  do {

    if (i < this->fill && this->bag[i] != NULL) {

      c = this->bag[i]->combine(c);
      this->bag[i] = NULL;

    } else {

      this->bag[i] = c;
      // this->fill = this->fill > i+1 ? this->fill : i+1;
      this->fill = MAX(this->fill, i + 1);
      return;
    }

    ++i;

  } while (i < BAG_SIZE);

  this->fill = BAG_SIZE;
}

template <typename T> void Bag<T>::merge(Bag<T> *that) {
  Pennant<T> *c = NULL;
#if FILLING_ARRAY
  T *carry = NULL;
#endif // FILLING_ARRAY
  char x;
  int i;

  // Deal with the partially-filled Pennants
  if (this->size < that->size) {
    i = this->size - (BLK_SIZE - that->size);

    if (i >= 0) {
#if FILLING_ARRAY
      memcpy(that->filling + that->size, this->filling + i,
             (BLK_SIZE - that->size) * sizeof(T));

      carry = that->filling;
#else
      memcpy(that->filling->els + that->size, this->filling->els + i,
             (BLK_SIZE - that->size) * sizeof(T));

      c = that->filling;
#endif // FILLING_ARRAY
      this->size = i;
    } else {
      // if (this->size > 0) {
#if FILLING_ARRAY
      memcpy(that->filling + that->size, this->filling, this->size * sizeof(T));
#else
      memcpy(that->filling->els + that->size, this->filling->els,
             this->size * sizeof(T));
#endif // FILLING_ARRAY
       //}
#if FILLING_ARRAY
      delete[] this->filling;
#else
      delete this->filling;
#endif // FILLING_ARRAY

      this->filling = that->filling;
      this->size += that->size;
    }

  } else {
    i = that->size - (BLK_SIZE - this->size);

    if (i >= 0) {

#if FILLING_ARRAY
      memcpy(this->filling + this->size, that->filling + i,
             (BLK_SIZE - this->size) * sizeof(T));

      carry = this->filling;
#else
      memcpy(this->filling->els + this->size, that->filling->els + i,
             (BLK_SIZE - this->size) * sizeof(T));

      c = this->filling;
#endif // FILLING_ARRAY

      this->filling = that->filling;
      this->size = i;
    } else {

      // if (that->size > 0) {
#if FILLING_ARRAY
      memcpy(this->filling + this->size, that->filling, that->size * sizeof(T));
#else
      memcpy(this->filling->els + this->size, that->filling->els,
             that->size * sizeof(T));
#endif // FILLING_ARRAY
       //}
      this->size += that->size;
    }
  }

  that->filling = NULL;

  // Update this->fill (assuming no final carry)
  // uint32_t min, max;
  int min, max;
  if (this->fill > that->fill) {
    min = that->fill;
    max = this->fill;
  } else {
    min = this->fill;
    max = that->fill;
  }

#if FILLING_ARRAY
  if (carry != NULL)
    c = new Pennant<T>(carry);
#endif // FILLING_ARRAY

  // Merge
  for (i = 0; i < min; ++i) {

    x = (this->bag[i] != NULL) << 2 |
        (i < that->fill && that->bag[i] != NULL) << 1 | (c != NULL);

    switch (x) {
    case 0x0:
      this->bag[i] = NULL;
      c = NULL;
      that->bag[i] = NULL;
      break;
    case 0x1:
      this->bag[i] = c;
      c = NULL;
      that->bag[i] = NULL;
      break;
    case 0x2:
      this->bag[i] = that->bag[i];
      that->bag[i] = NULL;
      c = NULL;
      break;
    case 0x3:
      c = that->bag[i]->combine(c);
      that->bag[i] = NULL;
      this->bag[i] = NULL;
      break;
    case 0x4:
      /* this->bag[i] = this->bag[i]; */
      c = NULL;
      that->bag[i] = NULL;
      break;
    case 0x5:
      c = this->bag[i]->combine(c);
      this->bag[i] = NULL;
      that->bag[i] = NULL;
      break;
    case 0x6:
      c = this->bag[i]->combine(that->bag[i]);
      that->bag[i] = NULL;
      this->bag[i] = NULL;
      break;
    case 0x7:
      /* this->bag[i] = this->bag[i]; */
      c = that->bag[i]->combine(c);
      that->bag[i] = NULL;
      break;
    default:
      break;
    }
  }

  that->fill = 0;

  if (this->fill == max) {
    if (c == NULL)
      return;

    do {
      if (i < max && this->bag[i] != NULL) {

        c = this->bag[i]->combine(c);
        this->bag[i] = NULL;

      } else {

        this->bag[i] = c;
        // this->fill = max > i+1 ? max : i+1;
        this->fill = MAX(max, i + 1);
        return;
      }
      // switch(i < max && this->bag[i] != NULL) {
      // case false:
      //   this->bag[i] = c;
      //   this->fill = max > i+1 ? max : i+1;
      //   return;
      // case true:
      //   c = this->bag[i]->combine(c);
      //   this->bag[i] = NULL;
      //   break;
      // default: break;
      // }

      ++i;

    } while (i < BAG_SIZE);
  } else { // that->fill == max
    int j;
    if (c == NULL) {
      this->fill = max;
      for (j = i; j < this->fill; ++j) {
        this->bag[j] = that->bag[j];
        that->bag[j] = NULL;
      }
      return;
    }

    do {
      if (i < max && that->bag[i] != NULL) {

        c = that->bag[i]->combine(c);
        this->bag[i] = NULL;
        that->bag[i] = NULL;

      } else {

        this->bag[i] = c;
        // this->fill = max > i+1 ? max : i+1;
        this->fill = MAX(max, i + 1);

        for (j = i + 1; j < this->fill; ++j) {
          this->bag[j] = that->bag[j];
          that->bag[j] = NULL;
        }
        return;
      }
      // switch(i < max && that->bag[i] != NULL) {
      // case false:
      //   this->bag[i] = c;
      //   this->fill = max > i+1 ? max : i+1;
      //   for (j = i+1; j < this->fill; ++j) {
      //     this->bag[j] = that->bag[j];
      //     that->bag[j] = NULL;
      //   }
      //   return;
      // case true:
      //   c = that->bag[i]->combine(c);
      //   this->bag[i] = NULL;
      //   that->bag[i] = NULL;
      //   break;
      // default: break;
      // }

      ++i;

    } while (i < BAG_SIZE);
  }

  this->fill = BAG_SIZE;
}

template <typename T> inline bool Bag<T>::split(Pennant<T> **p) {
  if (this->fill == 0) {
    *p = NULL;
    return false;
  }

  this->fill--;
  *p = this->bag[this->fill];
  this->bag[this->fill] = NULL;

  for (/*this->fill*/; this->fill > 0; this->fill--) {
    if (this->bag[this->fill - 1] != NULL)
      break;
  }
  return true;
}

template <typename T> int Bag<T>::split(Pennant<T> **p, int pos) {
  if (pos < 0 || this->fill <= pos) {
    *p = NULL;
    return this->fill - 1;
  }

  *p = this->bag[pos];

  for (int i = pos - 1; i >= 0; i--) {
    if (this->bag[i] != NULL)
      return i;
  }

  return -1;
}

template <typename T> inline uint32_t Bag<T>::getFill() const {
  return this->fill;
}

template <typename T> inline bool Bag<T>::isEmpty() const {
  return this->fill == 0 && this->size == 0;
}

template <typename T> inline Pennant<T> *Bag<T>::getFirst() const {
  return this->bag[0];
}

template <typename T> inline T *Bag<T>::getFilling() const {
#if FILLING_ARRAY
  return this->filling;
#else
  return this->filling->els;
#endif // FILLING_ARRAY
}

template <typename T> inline uint32_t Bag<T>::getFillingSize() const {
  return this->size;
}

template <typename T> inline void Bag<T>::clear() {
  this->fill = 0;
  this->size = 0;
}

#endif
