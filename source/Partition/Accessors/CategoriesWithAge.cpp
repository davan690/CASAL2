/**
 * @file CategoriesWithAge.cpp
 * @author  Scott Rasmussen (scott.rasmussen@zaita.com)
 * @version 1.0
 * @date 28/01/2013
 * @section LICENSE
 *
 * Copyright NIWA Science �2013 - www.niwa.co.nz
 *
 * $Date: 2008-03-04 16:33:32 +1300 (Tue, 04 Mar 2008) $
 */

// Headers
#include "CategoriesWithAge.h"

#include "Categories/Categories.h"
#include "Partition/Partition.h"

// Namespaces
namespace isam {
namespace partition {
namespace accessors {

/**
 * Default constructor
 */
CategoriesWithAge::CategoriesWithAge(const vector<string>& category_names, const unsigned& age) {
  // Variables
  model_ = Model::Instance();
  unsigned start_year = model_->start_year();
  unsigned final_year = model_->final_year();

  for (string category_name : category_names) {
    partition::Category& category = Partition::Instance().category(category_name);

    for (unsigned year = start_year; year <= final_year; ++year) {
      if (std::find(category.years_.begin(), category.years_.end(), year) == category.years_.end())
        continue; // Not valid in this year

      std::pair<string, Double*> target;
      target.first  = category_name;
      target.second = &category.data_[age - category.min_age_];
      data_[year].push_back(target);
    }
  }
}

/**
 * Return the number of active categories for the current year
 */
unsigned CategoriesWithAge::size() {
  return data_[model_->current_year()].size();
}

/**
 * Return an interator to the first element in our vector
 * for the current year.
 */
CategoriesWithAge::DataType::iterator CategoriesWithAge::begin() {
  return data_[model_->current_year()].begin();
}

/**
 * Return the "end" value for our data type iterator so it can
 * be compared to the begin iterator that was given to
 * any process.
 */
CategoriesWithAge::DataType::iterator CategoriesWithAge::end() {
  return data_[model_->current_year()].end();
}

} /* namespace accessors */
} /* namespace partition */
} /* namespace isam */
