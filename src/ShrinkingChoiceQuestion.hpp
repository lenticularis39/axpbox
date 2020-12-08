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
 * Configuration file creator.
 *
 * $Id: ShrinkingChoiceQuestion.h,v 1.1 2008/03/28 21:57:00 iamcamiel Exp $
 *
 * X-1.1        Camiel Vanderhoeven                             28-MAR-2008
 *      File created.
 **/

/**
 * Question class implementing a "Shrinking Choice" question.
 *
 * A shrinking choice question is a multiple choice question
 * where each answer given is removed from the list of valid
 * answers.
 **/
class ShrinkingChoiceQuestion : public MultipleChoiceQuestion {
  /**
   * Overridden to remove the answer that was given from the
   * allowed answer set.
   **/
  virtual void haveChosen(string choice) { dropChoice(choice); }
};
