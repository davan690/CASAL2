/*
 * AgeingErrorMatrix.cpp
 *
 *  Created on: 4/09/2013
 *      Author: Admin
 */

#include "AgeingErrorMatrix.h"

#include "AgeingErrors/Manager.h"

namespace niwa {
namespace reports {
namespace age {

/**
 * Default constructor
 */
AgeingErrorMatrix::AgeingErrorMatrix(Model* model) : Report(model) {
  run_mode_    = (RunMode::Type)(RunMode::kBasic | RunMode::kProjection);
  model_state_ = State::kFinalise;

  parameters_.Bind<string>(PARAM_AGEING_ERROR, &ageingerror_label_, "The ageing error label", "");
}

/**
 * Build
 */
void AgeingErrorMatrix::DoBuild() {
  ageingerror_ = model_->managers().ageing_error()->GetAgeingError(ageingerror_label_);
  if (!ageingerror_)
    LOG_ERROR_P(PARAM_AGEING_ERROR) << "Ageing error label (" << ageingerror_label_ << ") was not found.";
  }

/**
 * Execute the report
 */
void AgeingErrorMatrix::DoExecute() {
  LOG_TRACE();

  vector<vector<Double>>& mis_matrix = ageingerror_->mis_matrix();

  cache_ << "*"<< type_ << "[" << label_ << "]" << "\n";
  cache_ << "values "<< REPORT_R_MATRIX<<"\n";

  for (unsigned i = 0; i < mis_matrix.size(); ++i) {
    for (unsigned j = 0; j < mis_matrix[i].size(); ++j) {
      cache_ << AS_VALUE(mis_matrix [i][j]) << " ";
      if ( j == (mis_matrix[i].size() - 1))
        cache_ << "\n";
    }
  }

  ready_for_writing_ = true;
}

} /* namespace age */
} /* namespace reports */
} /* namespace niwa */
