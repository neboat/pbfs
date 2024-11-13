// Copyright (c) 2010, Tao B. Schardl
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

#ifndef BAG_CPP
#define BAG_CPP

#include <stdlib.h>
#include <string.h>

#include "bag.h"

//////////////////////////////////
///                            ///
/// Pennant method definitions ///
///                            ///
//////////////////////////////////
template <typename T>
Pennant<T>::Pennant()
{
  this->els = new T[BLK_SIZE];
  this->l = NULL;
  this->r = NULL;
}

template <typename T>
Pennant<T>::~Pennant()
{
//   int id = cilk::current_worker_id();
//   pfl->addPennant(id, this);
  delete[] els;

//   if (l != NULL)
//     delete l;
//   if (r != NULL)
//     delete r;
}

template <typename T>
inline const T* const
Pennant<T>::getElements()
{
  return this->els;
}

template <typename T>
inline Pennant<T>*
Pennant<T>::getLeft()
{
  return l;
}

template <typename T>
inline Pennant<T>*
Pennant<T>::getRight()
{
  return r;
}

/*
 * This method assumes that the pennant rooted at <that> has the
 * same number of nodes as the pennant rooted at <this>.
 */
template <typename T>
inline Pennant<T>*
Pennant<T>::combine(Pennant<T>* that)
{
  that->r = this->l;
  this->l = that;

  return this;
}

/*
 * This method performs the inverse operation of
 * Pennant<T>::combine and places half of the split here and the
 * the other half at the returned pointer
 */
template <typename T>
inline Pennant<T>*
Pennant<T>::split()
{
  Pennant<T>* that;

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
template <typename T>
Bag<T>::Bag() : fill(0), size(0)
{
  this->bag = new Pennant<T>*[BAG_SIZE];
  this->filling = new Pennant<T>();
}

/*
 * Copy Constructor. Performs a shallow bag copy.
 * Useful for verifying Bag implementation correctness, since
 * the split method destroys the Bag.
 */
template <typename T>
Bag<T>::Bag(Bag<T> *that) : fill(that->fill), size(that->size)
{
  this->bag = new Pennant<T>*[BAG_SIZE];
  for (int i = 0; i < BAG_SIZE; i++)
    this->bag[i] = that->bag[i];

  this->filling = that->filling;
}

template <typename T>
Bag<T>::~Bag()
{
  if (this->bag != NULL) {
    for (int i = 0; i < this->fill; i++) {
      if (this->bag[i] != NULL) {
	delete this->bag[i];
	this->bag[i] = NULL;
      }
    }

    delete[] this->bag;
  }

  delete this->filling;
}

template <typename T>
inline int
Bag<T>::numElements() const
{
  int count = this->size;
  int k = 1;
  for (int i = 0; i < this->fill; i++) {
    if (this->bag[i] != NULL)
      count += k*BLK_SIZE;
    k = k * 2;
  }

  return count;
}

template <typename T>
inline void
Bag<T>::insert(T el)
{
  if (this->size < BLK_SIZE) {
    this->filling->els[this->size++] = el;
    return;
  }

  Pennant<T> *c = this->filling;
  this->filling = new Pennant<T>();
  this->filling->els[0] = el;
  this->size = 1;

  int i = 0;

  do {
    switch(i < this->fill && this->bag[i] != NULL) {
    case false:
      this->bag[i] = c;
      this->fill = this->fill > i+1 ? this->fill : i+1;
      return;
    case true:
      c = this->bag[i]->combine(c);
      this->bag[i] = NULL;
      break;
    default: break;
    }

    ++i;

  } while (i < BAG_SIZE);

  this->fill = BAG_SIZE;
}

template <typename T>
void
Bag<T>::merge(Bag<T>* that)
{
  Pennant<T> *c = NULL;
  char x;
  int i;

  // Deal with the partially-filled Pennants
  if (this->size < that->size) {
    i = this->size - (BLK_SIZE - that->size);

    if (i >= 0) {

      memcpy(that->filling->els + that->size,
             this->filling->els + i,
             (BLK_SIZE - that->size) * sizeof(T));

      c = that->filling;
      this->size = i;
    } else {

      if (this->size > 0)
	memcpy(that->filling->els + that->size,
	       this->filling->els,
	       this->size * sizeof(T));

      delete this->filling;

      this->filling = that->filling;
      this->size += that->size;
    }

  } else {
    i = that->size - (BLK_SIZE - this->size);

    if (i >= 0) {

      memcpy(this->filling->els + this->size,
             that->filling->els + i,
             (BLK_SIZE - this->size) * sizeof(T));

      c = this->filling;
      this->filling = that->filling;
      this->size = i;
    } else {

      if (that->size > 0)
	memcpy(this->filling->els + this->size,
	       that->filling->els,
	       that->size * sizeof(T));

      this->size += that->size;
    }
  }

  that->filling = NULL;

  // Update this->fill (assuming no final carry)
  int min, max;
  if (this->fill > that->fill) {
    min = that->fill;
    max = this->fill;
  } else {
    min = this->fill;
    max = that->fill;
  }

  // Merge
  for (i = 0; i < min; ++i) {

    x =
      (this->bag[i] != NULL) << 2 |
      (i < that->fill && that->bag[i] != NULL) << 1 |
      (c != NULL);

    switch(x) {
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
    default: break;
    }
  }

  that->fill = 0;

  if (this->fill == max) {
    if (c == NULL)
      return;

    do {
      switch(i < max && this->bag[i] != NULL) {
      case false:
	this->bag[i] = c;
	this->fill = max > i+1 ? max : i+1;
	return;
      case true:
	c = this->bag[i]->combine(c);
	this->bag[i] = NULL;
	break;
      default: break;
      }
      
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
      switch(i < max && that->bag[i] != NULL) {
      case false:
	this->bag[i] = c;
	this->fill = max > i+1 ? max : i+1;
	for (j = i+1; j < this->fill; ++j) {
	  this->bag[j] = that->bag[j];
	  that->bag[j] = NULL;
	}
	return;
      case true:
	c = that->bag[i]->combine(c);
	this->bag[i] = NULL;
	that->bag[i] = NULL;
	break;
      default: break;
      }
      
      ++i;
      
    } while (i < BAG_SIZE);    
  }

   this->fill = BAG_SIZE;
}

template <typename T>
inline bool
Bag<T>::split(Pennant<T>* *p)
{
  if (this->fill == 0) {
    *p = NULL;
    return false;
  }

  this->fill--;
  *p = this->bag[this->fill];
  this->bag[this->fill] = NULL;

  for (this->fill; this->fill > 0; this->fill--) {
    if (this->bag[this->fill-1] != NULL)
      break;
  }
  return true;
}

template <typename T>
int
Bag<T>::split(Pennant<T>* *p, int pos)
{
  if (pos < 0 || this->fill <= pos) {
    *p = NULL;
    return this->fill - 1;
  }

  *p = this->bag[pos];

  for (int i = pos-1; i >= 0; i--) {
    if (this->bag[i] != NULL)
      return i;
  }

  return -1;
}

template <typename T>
inline int
Bag<T>::getFill() const
{
  return this->fill;
}

template <typename T>
inline bool
Bag<T>::isEmpty() const
{
  return this->fill == 0 && this->size == 0;
}

template <typename T>
inline Pennant<T>*
Bag<T>::getFirst() const
{
  return this->bag[0];
}

template <typename T>
inline Pennant<T>*
Bag<T>::getFilling() const
{
  return this->filling;
}

template <typename T>
inline int
Bag<T>::getFillingSize() const
{
  return this->size;
}

template <typename T>
inline void
Bag<T>::clear()
{
  this->fill = 0;
  this->size = 0;
}

#endif
