/* AXPbox Alpha Emulator
 * Copyright (C) 2020 Tomáš Glozar
 * Website: https://github.com/lenticularis39/axpbox
 *
 * Forked from: ES40 emulator
 * Copyright (C) 2007-2008 by the ES40 Emulator Project
 * Copyright (C) 2007 by Camiel Vanderhoeven
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
 * Question class for Configuration file creator.
 *
 * $Id: Question.h,v 1.1 2008/03/28 21:56:59 iamcamiel Exp $
 *
 * X-1.1        Camiel Vanderhoeven                             28-MAR-2008
 *      File created.
 **/
#if !defined(INCLUDED_QUESTION_H)
#define INCLUDED_QUESTION_H

/**
 * Abstract Question base class.
 **/
class Question {
public:
  /**
   * Return the answer previously given.
   **/
  string getAnswer() { return mAnswer; }

  /**
   * Set the answer.
   **/
  void setAnswer(string answer) { mAnswer = answer; }

  /**
   * Define an explanation for the question, that will be shown when
   * the question is answered with '?'.
   **/
  void setExplanation(string explanation) { mExplanation = explanation; }

  /**
   * Define the question to ask.
   **/
  void setQuestion(string question) { mQuestion = question; }

  /**
   * Define a default value to use when the question is answered
   * with a <return>.
   **/
  void setDefault(string defval) { mDefault = defval; }

  /**
   * Ask the question, and return the value given.
   **/
  virtual string ask() = 0;

  /**
   * Display the explanation.
   **/
  virtual void explain() {
    cout << "\nEXPLANATION:\n" << mExplanation << "\n\n";
  }

protected:
  /** The stored answer. */
  string mAnswer;

  /** An explanation for the question. */
  string mExplanation;

  /** The question to ask. */
  string mQuestion;

  /** Default value. */
  string mDefault;
};

#endif // !defined(INCLUDED_QUESTION_H)
