/* ES40 emulator.
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 *
 * WWW    : http://sourceforge.net/projects/es40
 * E-mail : camiel@camicom.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * Although this is not required, the author would appreciate being notified of,
 * and receiving any modifications you may make to the source code that might
 * serve the general public.
 */

/**
 * \file
 * NumberQuestion class for Configuration file creator.
 *
 * $Id: NumberQuestion.h,v 1.1 2008/03/28 21:56:58 iamcamiel Exp $
 *
 * X-1.1        Camiel Vanderhoeven                             28-MAR-2008
 *      File created.
 **/

/**
 * Convert an integer to a string.
 **/
inline string i2s(int x) {
  ostringstream o;
  o << x;
  return o.str();
}

/**
 * Convert a string to an integer.
 *
 * Throws a CLogicException when the input is not numeric.
 **/
inline int s2i(const string x) {
  istringstream i(x);
  int x1;
  char c;
  if (!(i >> x1) || i.get(c))
    FAILURE(Logic, "invalid conversion");
  ;
  return x1;
}

/**
 * Question class that accepts a numeric answer within
 * a defined range.
 **/
class NumberQuestion : public FreeTextQuestion {
public:
  /**
   * Define the allowable range for the answer.
   **/
  void setRange(int low, int high) {
    mLow = low;
    mHigh = high;
  }

  /**
   * Convert the answer to a number.
   **/
  int getNum() { return s2i(mAnswer); }

  /**
   * Ask the question and return the answer.
   **/
  virtual string ask() {
    /* Set the options list to (low-high).
     */
    setOptions(i2s(mLow) + "-" + i2s(mHigh));

    /* Keep repeating the question until a valid
     * answer is received.
     */
    for (;;) {
      int value;

      /* Ask the question as a FreeTextQuestion. This
       * takes care of the explanation and default
       * value handling.
       */
      FreeTextQuestion::ask();

      try {
        /* Convert the answer to an integer.
         */
        value = s2i(mAnswer);

        /* If the answer is within the allowed range,
         * return it.
         */
        if (value >= mLow && value <= mHigh)
          return mAnswer;

        /* The answer is out of range.
         */
        cout << "\nPlease enter a value that is within the indicated range, or "
                "'?' for help.\n\n";
      } catch (CLogicException) {
        /* The answer is not a number.
         */
        cout << "\nPlease enter an integer value.\n\n";
      }
    }
  }

protected:
  /** Low limit of the allowed range. */
  int mLow;
  /** High limit of the allowed range. */
  int mHigh;
};
