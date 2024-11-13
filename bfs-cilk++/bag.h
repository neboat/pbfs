// -*- C++ -*-
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

#ifndef BAG_H
#define BAG_H

#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <cilk.h>

const int BAG_SIZE = 64;
const int BLK_SIZE = 128;

extern "C++" {

  template <typename T> class Bag;
  template <typename T> class Bag_reducer;

  template <typename T>
  class Pennant
  {
  private:
    T* els;
    Pennant<T> *l, *r;
    
  public:
    Pennant();
    ~Pennant();
    
    inline const T* const getElements();
    inline Pennant<T>* getLeft();
    inline Pennant<T>* getRight();

    inline void clearChildren() {
      l = NULL;
      r = NULL;
    }

    inline Pennant<T>* combine(Pennant<T>*);
    inline Pennant<T>* split();
    
    friend class Bag<T>;
    friend class Bag_reducer<T>;
  };
  
  template <typename T>
  class Bag
  {
  private:
    // fill points to the bag entry one position beyond the MSP
    int fill;
    Pennant<T>* *bag;
    Pennant<T>* filling;
    // size points to the filling array entry on position
    // beyond last valid element.
    int size;
    
  public:
    Bag();
    Bag(Bag<T>*);
    
    ~Bag();
    
    inline void allocBag()
    {
      this->bag = new Pennant<T>*[BAG_SIZE];
      for (int i = 0; i < BAG_SIZE; ++i)
	this->bag[i] = NULL;
    }
    
    inline void allocFilling()
    {
      this->filling = new Pennant<T>();
    }
    
    inline void insert(T);
    void merge(Bag<T>*);
    inline bool split(Pennant<T>**);
    int split(Pennant<T>**, int);
    
    inline int numElements() const;
    inline int getFill() const;
    inline bool isEmpty() const;
    inline Pennant<T>* getFirst() const;
    inline Pennant<T>* getFilling() const;
    inline int getFillingSize() const;
    
    void clear();
    
    friend class Bag_reducer<T>;
    friend class cilk::monoid_base< Bag<T> >;
  };
  
  template <typename T>
  class Bag_reducer
  {
  public:
    struct Monoid: cilk::monoid_base< Bag<T> >
    {
      static void reduce (Bag<T> *left, Bag<T> *right) {
	left->merge(right);
      }
    };
    
  private:
    cilk::reducer<Monoid> imp_;
    
  public:
    Bag_reducer();
    
    void insert(T);
    void merge(Bag_reducer<T>*);
    inline bool split(Pennant<T>**);
    int split(Pennant<T>**, int);
    
    inline Bag<T> &get_reference();
    
    inline int numElements() const;
    inline int getFill() const;
    inline bool isEmpty() const;
    inline Pennant<T>* getFirst() const;
    inline Pennant<T>* getFilling() const;
    inline int getFillingSize() const;
    
    inline void clear();
  };

  template <typename T>
  Bag_reducer<T>::Bag_reducer() : imp_() { }

  template <typename T>
  void
  Bag_reducer<T>::insert(T el)
  {
    imp_.view().insert(el);
  }

  template <typename T>
  void
  Bag_reducer<T>::merge(Bag_reducer<T>* that)
  {
    this->imp_.view().merge(that->imp_.view());
  }

  template <typename T>
  inline bool
  Bag_reducer<T>::split(Pennant<T>* *p)
  {
    return imp_.view().split(p);
  }

  template <typename T>
  int
  Bag_reducer<T>::split(Pennant<T>* *p, int pos)
  {
    return imp_.view().split(p, pos);
  }

  template <typename T>
  inline Bag<T>&
  Bag_reducer<T>::get_reference()
  {
    return imp_.view();
  }

  template <typename T>
  inline int
  Bag_reducer<T>::numElements() const
  {
    return imp_.view().numElements();
  }

  template <typename T>
  inline int
  Bag_reducer<T>::getFill() const
  {
    return imp_.view().getFill();
  }

  template <typename T>
  inline bool
  Bag_reducer<T>::isEmpty() const
  {
    return imp_.view().isEmpty();
  }

  template <typename T>
  inline Pennant<T>*
  Bag_reducer<T>::getFirst() const
  {
    return imp_.view().getFirst();
  }

  template <typename T>
  inline Pennant<T>*
  Bag_reducer<T>::getFilling() const
  {
    return imp_.view().getFilling();
  }

  template <typename T>
  inline int
  Bag_reducer<T>::getFillingSize() const
  {
    return imp_.view().getFillingSize();
  }

  template <typename T>
  inline void
  Bag_reducer<T>::clear()
  {
    imp_.view().clear();
  }
}

#include "bag.cpp"

#endif
