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
 * MultipleChoiceQuestion class for Configuration file creator.
 *
 * $Id: MultipleChoiceQuestion.h,v 1.1 2008/03/28 21:56:58 iamcamiel Exp $
 *
 * X-1.1        Camiel Vanderhoeven                             28-MAR-2008
 *      File created.
 **/

/**
 * Class defining a possible answer to a multiple
 * choice question.
 **/
class Answer {
public:
  /**
   * Constructor.
   *
   * Defines the answer, value and explanation of
   * this answer.
   **/
  Answer(string answer, string value, string explanation) {
    mAnswer = answer;
    mValue = value;
    mExplanation = explanation;
  }

  /**
   * Equality operator. Matches on Answer.
   **/
  bool operator==(string x) { return (mAnswer == x); }
  /**
   * Get the answer.
   **/
  string getAnswer() { return mAnswer; }

  /**
   * Get the value.
   **/
  string getValue() { return mValue; }

  /**
   * Get the explanation.
   **/
  string getExplanation() { return mExplanation; }

protected:
  /** Answer as it should be entered by the user. */
  string mAnswer;

  /** Value as it is output to the config file. */
  string mValue;

  /** Explanation shown when '?' is given as an answer. */
  string mExplanation;
};

/**
 * Vector containing all possible answers to a multiple
 * choice question.
 **/
typedef vector<Answer> AnswerSet;

/**
 * Question class implementing a multiple choice question.
 **/
class MultipleChoiceQuestion : public FreeTextQuestion {
public:
  /**
   * Add an answer to the set of allowed answers.
   **/
  void addAnswer(string answer, string value, string explanation) {
    mAnswerSet.push_back(Answer(answer, value, explanation));
  }

  /**
   * Explain the question.
   **/
  virtual void explain() {
    AnswerSet::iterator itAnswers;

    /* Explain the quistion as usual.
     */
    FreeTextQuestion::explain();

    /* Explain the allowed answers.
     */
    cout << "POSSIBLE VALUES:\n";

    /* Iterate through the answer set.
     */
    for (itAnswers = mAnswerSet.begin(); itAnswers != mAnswerSet.end();
         itAnswers++) {
      /* Print the answer and its explanation.
       */
      cout << itAnswers->getAnswer() << "\t\t" << itAnswers->getExplanation()
           << "\n";
    }
    cout << "\n";
  }

  /**
   * Get the number of answers in the answer set.
   **/
  size_t countAnswers() { return mAnswerSet.size(); }

  /**
   * Ask the question.
   **/
  virtual string ask() {
    AnswerSet::iterator itAnswers;
    string options;

    /* Iterate through the answer set to create the
     * options list.
     */

    for (itAnswers = mAnswerSet.begin(); itAnswers != mAnswerSet.end();
         itAnswers++) {
      if (options != "")
        options += ",";
      options += itAnswers->getAnswer();
    }

    /* Set the options list.
     */
    setOptions(options);

    /* Keep repeating the question until a valid
     * answer is received.
     */
    for (;;) {
      /* Ask the question as a FreeTextQuestion. This
       * takes care of the explanation and default
       * value handling.
       */
      FreeTextQuestion::ask();

      /* Try to find the answer received in the answer
       * set.
       */
      itAnswers = find(mAnswerSet.begin(), mAnswerSet.end(), mAnswer);

      /* Has a matching answer be found?
       */
      if (itAnswers != mAnswerSet.end()) {
        /* Get the value for the received answer.
         */
        string value = itAnswers->getValue();
        haveChosen(mAnswer);
        mAnswer = value;

        /* Return the value.
         */
        return mAnswer;
      }

      /* No valid, matching answer received.
       */
      cout << "\nPlease enter a valid value, or '?' for help.\n\n";
    }
  };

  /**
   * Remove one answer from the answer set.
   **/
  void dropChoice(string choice) {
    AnswerSet::iterator itAnswers;

    /* Try to find the answer in the answer set.
     */
    itAnswers = find(mAnswerSet.begin(), mAnswerSet.end(), mAnswer);

    /* Has a matching answer be found?
     */
    if (itAnswers != mAnswerSet.end()) {
      /* Delete the answer from the list.
       */
      mAnswerSet.erase(itAnswers);
    }
  }

  /**
   * Override this to do something special with
   * the answer received.
   **/
  virtual void haveChosen(string choice) {}

  /**
   * Return the first possible answer.
   **/
  string getFirstChoice() { return mAnswerSet.begin()->getAnswer(); }

protected:
  /** Set containing all allowed answers. */
  AnswerSet mAnswerSet;
};
