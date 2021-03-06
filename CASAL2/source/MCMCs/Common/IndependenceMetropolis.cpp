/**
 * @file IndependenceMetropolis.cpp
 * @author Scott Rasmussen (scott.rasmussen@zaita.com)
 * @github https://github.com/Zaita
 * @date 21/05/2015
 * @section LICENSE
 *
 * Copyright NIWA Science �2015 - www.niwa.co.nz
 *
 */

// headers
#include <MCMCs/Common/IndependenceMetropolis.h>
#include "Estimates/Manager.h"
#include "EstimateTransformations/Manager.h"
#include "Model/Model.h"
#include "Minimisers/Manager.h"
#include "ObjectiveFunction/ObjectiveFunction.h"
#include "Reports/Manager.h"
#include "Utilities/DoubleCompare.h"
#include "Utilities/RandomNumberGenerator.h"

// namespaces
namespace niwa {
namespace mcmcs {

namespace dc = niwa::utilities::doublecompare;

/**
 * Default constructor
 */
IndependenceMetropolis::IndependenceMetropolis(Model* model) : MCMC(model) {
  parameters_.Bind<double>(PARAM_START, &start_, "The covariance multiplier for the starting point of the MCMC", "", 0.0)->set_lower_bound(0.0);
  parameters_.Bind<unsigned>(PARAM_KEEP, &keep_, "The spacing between recorded values in the MCMC", "", 1u)->set_lower_bound(1u);
  parameters_.Bind<double>(PARAM_MAX_CORRELATION, &max_correlation_, "The maximum absolute correlation in the covariance matrix of the proposal distribution", "", 0.8)->set_range(0.0, 1.0, false, true);
  parameters_.Bind<string>(PARAM_COVARIANCE_ADJUSTMENT_METHOD, &correlation_method_, "The method for adjusting small variances in the covariance proposal matrix"
      , "", PARAM_CORRELATION)->set_allowed_values({PARAM_COVARIANCE, PARAM_CORRELATION, PARAM_NONE});
  parameters_.Bind<double>(PARAM_CORRELATION_ADJUSTMENT_DIFF, &correlation_diff_, "The minimum non-zero variance times the range of the bounds in the covariance matrix of the proposal distribution", "", 0.0001)->set_lower_bound(0.0, false);
  parameters_.Bind<string>(PARAM_PROPOSAL_DISTRIBUTION, &proposal_distribution_, "The shape of the proposal distribution (either the t or the normal distribution)", "", PARAM_T);
  parameters_.Bind<unsigned>(PARAM_DF, &df_, "The degrees of freedom of the multivariate t proposal distribution", "", 4)->set_lower_bound(0, false);
  parameters_.Bind<unsigned>(PARAM_ADAPT_STEPSIZE_AT, &adapt_step_size_, "The iteration numbers in which to check and resize the MCMC stepsize", "", true)->set_lower_bound(0);
  parameters_.Bind<unsigned>(PARAM_ADAPT_COVARIANCE_AT, &adapt_covariance_matrix_, "The iteration numbers in which to adapt the covariance matrix", "", true)->set_lower_bound(0);
  parameters_.Bind<string>(PARAM_ADAPT_STEPSIZE_METHOD, &adapt_stepsize_method_, "The method to use to adapt the step size", "", PARAM_RATIO)->set_allowed_values({PARAM_RATIO, PARAM_DOUBLE_HALF});

  jumps_                          = 0;
  successful_jumps_               = 0;
  jumps_since_adapt_              = 0;
  successful_jumps_since_adapt_   = 0;
  last_item_                      = false;
}

/**
 * Get the covariance matrix from the minimiser and then
 * adjust it for the proposal distribution
 */
void IndependenceMetropolis::BuildCovarianceMatrix() {
  LOG_MEDIUM() << "Building covariance matrix";
  // Are we starting at MPD or recalculating the matrix based on an empirical sample
  ublas::matrix<double> original_correlation;
  if (recalculate_covariance_) {
    LOG_MEDIUM() << "Recalculating covariance matrix";
    covariance_matrix_ = covariance_matrix_lt;
  }

  // Remove for the shared library only used for debugging purposes
  // Minimiser* minimiser = model_->managers().minimiser()->active_minimiser();
  // covariance_matrix_ = minimiser->covariance_matrix();
  // original_correlation = minimiser->correlation_matrix();

  // This is already built by MPD.cpp at line 137. in the frontend the minimiser is dropped out before
  // the MCMC state kicks in, so this will return a rubbish covariance matrix

  if (correlation_method_ == PARAM_NONE)
    return;

  /**
   * Adjust the covariance matrix for the proposal distribution
   */
  LOG_MEDIUM() << "Printing covariance matrix before applying the correlation adjustment";
  for (unsigned i = 0; i < covariance_matrix_.size1(); ++i) {
    for (unsigned j = 0; j < covariance_matrix_.size2(); ++j) {
      LOG_MEDIUM() << "row = " << i + 1 << " col = " << j + 1 << " value = " << covariance_matrix_(i,j);
    }
  }
  ublas::matrix<double> original_covariance(covariance_matrix_);

  LOG_MEDIUM() << "Beginning correlation adjustment. rows = " << original_covariance.size1() << " cols = " << original_covariance.size2();
  // Q: is this section supposed to adjust the full matrix (except for the diagonal) or not? see pg. 67 (79) of User Manual (item #3)
  for (unsigned i = 0; i < (covariance_matrix_.size1() - 1); ++i) {
    for (unsigned j = i + 1; j < covariance_matrix_.size2(); ++j) {
      // This assumes that the lower and upper triangles match
      double value = original_covariance(i,j) / sqrt(original_covariance(i,i) * original_covariance(j,j));
      LOG_MEDIUM() << "row = " << i + 1 << " col = " << j + 1 << " correlation = " << value;
      if (original_covariance(i,j) / sqrt(original_covariance(i,i) * original_covariance(j,j)) > max_correlation_) {
        covariance_matrix_(i,j) = max_correlation_ * sqrt(original_covariance(i,i) * original_covariance(j,j));
        covariance_matrix_(j,i) = covariance_matrix_(i,j);
        LOG_FINE() << "adjusted lower: row = " << i + 1 << " col = " << j + 1;
      }
      if (original_covariance(i,j) / sqrt(original_covariance(i,i) * original_covariance(j,j)) < -max_correlation_){
        covariance_matrix_(i,j) = -max_correlation_ * sqrt(original_covariance(i,i) * original_covariance(j,j));
        covariance_matrix_(j,i) = covariance_matrix_(i,j);
        LOG_FINE() << "adjusted higher: row = " << i + 1 << " col = " << j + 1;
      }
    }
  }

  LOG_MEDIUM() << "Printing upper triangle of covariance matrix";
  for (unsigned i = 0; i < (covariance_matrix_.size1() - 1); ++i) {
    for (unsigned j = i + 1; j < covariance_matrix_.size2(); ++j) {
      LOG_MEDIUM() << "row = " << i + 1 << " col = " << j + 1 << " value = " << covariance_matrix_(i,j);
    }
  }

  /**
   * Adjust any non-zero variances less than min_diff_ * difference between bounds
   */
  vector<double> difference_bounds;
  vector<Estimate*> estimates = model_->managers().estimate()->GetIsEstimated();
  LOG_MEDIUM() << "upper_bound lower_bound";
  for (Estimate* estimate : estimates) {
    difference_bounds.push_back( estimate->upper_bound() - estimate->lower_bound() );
    LOG_MEDIUM() << estimate->upper_bound() << " " << estimate->lower_bound();
  }

  for (unsigned i = 0; i < covariance_matrix_.size1(); ++i) {
    if (covariance_matrix_(i,i) < (correlation_diff_ * difference_bounds[i]) && covariance_matrix_(i,i) != 0) {
      if (correlation_method_ == PARAM_COVARIANCE) {
        double multiply_covariance = (sqrt(correlation_diff_) * difference_bounds[i]) / sqrt(covariance_matrix_(i,i));
        LOG_MEDIUM() << "multiplier = " << multiply_covariance << " for parameter = " << i + 1;
        for (unsigned j = 0; j < covariance_matrix_.size2(); ++j) {
          covariance_matrix_(i,j) *= multiply_covariance;
          covariance_matrix_(j,i) *= multiply_covariance;
        }
      } else if(correlation_method_ == PARAM_CORRELATION) {
        covariance_matrix_(i,i) = correlation_diff_ * difference_bounds[i];
      }
    }
  }

  LOG_MEDIUM() << "Printing adjusted covariance matrix";
  for (unsigned i = 0; i < covariance_matrix_.size1(); ++i) {
    for (unsigned j = 0; j < covariance_matrix_.size2(); ++j) {
      LOG_MEDIUM() << "row = " << i + 1 << " col = " << j + 1 << " value = " << covariance_matrix_(i,j);
    }
  }

}

/**
 * Perform Cholesky decomposition on the covariance
 * matrix before it is used in the MCMC.
 *
 * @return true on success, false on failure
 */
bool IndependenceMetropolis::DoCholeskyDecmposition() {
  LOG_TRACE();
  if (covariance_matrix_.size1() != covariance_matrix_.size2())
      LOG_FATAL() << "Invalid covariance matrix (rows != columns). It must be a square matrix";
    unsigned matrix_size1 = covariance_matrix_.size1();
    covariance_matrix_lt = covariance_matrix_;

    for (unsigned i = 0; i < matrix_size1; ++i) {
      for (unsigned j = 0; j < matrix_size1; ++j) {
        covariance_matrix_lt(i,j) = 0.0;
      }
    }

    for (unsigned i = 0; i < matrix_size1; ++i) {
      covariance_matrix_lt(i,i) = 1.0;
    }

    if (covariance_matrix_(0,0) < 0) {
      return false;
    }

    double sum = 0.0;

    covariance_matrix_lt(0,0) = sqrt(covariance_matrix_(0,0));

    for (unsigned i = 1; i < matrix_size1; ++i)
      covariance_matrix_lt(i,0) = covariance_matrix_(i,0)/covariance_matrix_lt(0,0);

    for (unsigned i = 1; i < matrix_size1; ++i) {
      sum = 0.0;
      for (unsigned j = 0; j < i; ++j)
        sum += covariance_matrix_lt(i,j) * covariance_matrix_lt(i,j);

      if (covariance_matrix_(i,i) <= sum) {
        LOG_FATAL() << "Cholesky decomposition failed, Singular matrix found, 'covariance_matrix_(i,i) <= sum', for row and column "
          << i + 1 << " parameter = " << estimates_[i]->parameter() << " sum = " << sum << " value = " << covariance_matrix_(i,i);
        return false;
      }
      covariance_matrix_lt(i,i) = sqrt(covariance_matrix_(i,i) - sum);
      for (unsigned j = i+1; j < matrix_size1; ++j) {
        sum = 0.0;
        for (unsigned k = 0; k < i; ++k)
          sum += covariance_matrix_lt(j,k) * covariance_matrix_lt(i,k);
        covariance_matrix_lt(j,i) = (covariance_matrix_(j,i) - sum) / covariance_matrix_lt(i,i);
      }
    }

    sum = 0.0;
    for (unsigned i = 0; i < (matrix_size1 - 1); ++i)
      sum += covariance_matrix_lt(matrix_size1 - 1,i) * covariance_matrix_lt(matrix_size1-1,i);
    if (covariance_matrix_(matrix_size1 - 1, matrix_size1 - 1) <= sum) {
      LOG_FATAL() << "Cholesky decomposition failed,  Singular matrix found. 'covariance_matrix_(matrix_size1 - 1, matrix_size1 - 1) <= sum', for row and column "
        << matrix_size1 << " sum = " << sum << " value = " << covariance_matrix_(matrix_size1 - 1, matrix_size1 - 1);
      return false;
    }
    covariance_matrix_lt(matrix_size1 - 1, matrix_size1 - 1) = sqrt(covariance_matrix_(matrix_size1 - 1, matrix_size1 - 1) - sum);

   return true;
}

/**
 * Generate a set of random starting values for the estimated parameters
 */
void IndependenceMetropolis::GenerateRandomStart() {
  vector<Double> original_candidates = candidates_;
  vector<Estimate*> estimates = model_->managers().estimate()->GetIsEstimated();

  unsigned attempts = 0;
  bool candidates_pass = false;

  if (candidates_.size() != estimate_count_)
    LOG_CODE_ERROR() << "candidates_.size() != estimate_count_";

  do {
    candidates_pass = true;
    attempts++;
    if (attempts > 1000)
      LOG_FATAL() << "Failed to generate random start after 1000 attempts";

    candidates_ = original_candidates;
    FillMultivariateNormal(start_);
    for (unsigned i = 0; i < estimates.size(); ++i) {
      if (estimates[i]->lower_bound() > candidates_[i] || estimates[i]->upper_bound() < candidates_[i]) {
        candidates_pass = false;
        break;
      }
    }

  } while (!candidates_pass);
}

/**
 * Fill the candidates with an attempt using a multivariate normal distribution
 */
void IndependenceMetropolis::FillMultivariateNormal(double step_size) {
  utilities::RandomNumberGenerator& rng = utilities::RandomNumberGenerator::Instance();

  vector<double>  normals(estimate_count_ , 0.0);
  for (unsigned i = 0; i < estimate_count_; ++i) {
    normals[i] = rng.normal();
  }
  vector<double>  dv(estimate_count_, 0.0);

// Method from CASAL's algorithm
  for (unsigned i = 0; i < estimate_count_; ++i) {
    for (unsigned j = 0; j < estimate_count_; ++j) {
      dv[i] += covariance_matrix_lt(i, j) * normals[j];
      //LOG_MEDIUM() << "ndx =  " << j * estimate_count_ + i;
    }
    if (is_enabled_estimate_[i])
      candidates_[i] += dv[i] * step_size;
  }

// Original method from SPM
/*
  for (unsigned i = 0; i < estimate_count_; ++i) {
    Double row_sum = 0.0;
    for (unsigned j = 0; j < estimate_count_; ++j) {
      row_sum += covariance_matrix_lt(j, i) * normals[j];
    }

    if (is_enabled_estimate_[i])
      candidates_[i] += row_sum * step_size;
  }*/
}

/**
 * Fill the candidates with an attempt using a multivariate t-distribution
 */
void IndependenceMetropolis::FillMultivariateT(double step_size) {
  utilities::RandomNumberGenerator& rng = utilities::RandomNumberGenerator::Instance();

  vector<double>  normals(estimate_count_, 0.0);
  vector<double>  chisquares(estimate_count_, 0.0);
  for (unsigned i = 0; i < estimate_count_; ++i) {
    normals[i] = rng.normal();
    chisquares[i] = 1 / (rng.chi_squared(df_) / df_);
  }

  for (unsigned i = 0; i < estimate_count_; ++i) {
    double row_sum = 0.0;
    for (unsigned j = 0; j < estimate_count_; ++j) {
      row_sum += covariance_matrix_lt(i, j) * normals[j] * chisquares[j];
    }

    if (is_enabled_estimate_[i])
      candidates_[i] += row_sum * step_size;
  }
}

/**
 * Update the MCMC step size if it is required
 * This is done by
 * 1. Checking if the current iteration is in the adapt_step_size vector
 * 2. Modify the step size
 */
void IndependenceMetropolis::UpdateStepSize() {
  if (jumps_since_adapt_ > 0 && successful_jumps_since_adapt_ > 0) {
    if (std::find(adapt_step_size_.begin(), adapt_step_size_.end(), jumps_) == adapt_step_size_.end())
      return;

    if (adapt_stepsize_method_ == PARAM_RATIO) {
      // modify the stepsize so that AcceptanceRate = 0.24
      step_size_ *= ((double)successful_jumps_since_adapt_ / (double)jumps_since_adapt_) * 4.166667;
      // Ensure the stepsize remains positive
      step_size_ = dc::ZeroFun(step_size_, 1e-10);
      // reset counters
    } else if (adapt_stepsize_method_ == PARAM_DOUBLE_HALF) {
      // This is a half or double method really.
      double acceptance_rate;
      if ((double)successful_jumps_since_adapt_ == 0.0)
        acceptance_rate = double(successful_jumps_) / double(jumps_);
      else
        acceptance_rate = (double)successful_jumps_since_adapt_ / (double)jumps_since_adapt_;
      //LOG_MEDIUM() << "acceptance rate since last jump = " << acceptance_rate << " step size " << step_size_ << " numerator = " << ((Double)successful_jumps_ - (Double)successful_jumps_since_adapt_) << " denominator = " << ((Double)jumps_ - (Double)jumps_since_adapt_);
      if (acceptance_rate > 0.5)
        step_size_ *= 2;
      else if (acceptance_rate < 0.2)
        step_size_ /= 2;
      LOG_MEDIUM() << "new step_size = " << step_size_;
    }

    jumps_since_adapt_ = 0;
    successful_jumps_since_adapt_ = 0;
    return;
  }
}

/**
 * Update the MCMC covariance matrix if it is required
 * This is done by
 * 1. Checking if the current iteration is in the adapt_covariance_matrix vector
 * 2. Modify the covariance matrix
 */
void IndependenceMetropolis::UpdateCovarianceMatrix() {
  if (jumps_since_adapt_ > 1000) {
    if (std::find(adapt_covariance_matrix_.begin(), adapt_covariance_matrix_.end(), jumps_) == adapt_covariance_matrix_.end())
      return;

    recalculate_covariance_ = true;
    LOG_MEDIUM() << "Recalculating the covariance matrix after " << chain_.size() << " iterations";
    // modify the covaraince matrix this algorithm is stolen from CASAL, maybe not the best place to take it from

    //number of parameters
    int n_params = chain_[0].values_.size();

    // number of iterations
    int n_iter = chain_.size() - 1;
    LOG_MEDIUM() << "Number of parameters = " << n_params << ", number of iterations used to recalculate covariance = " << n_iter;

    // temp covariance matrix
    ublas::matrix<double> temp_covariance = covariance_matrix_;

    // Mean parameter vector
    vector<double> mean_var(n_params, 1.0);

    for (int i = 0; i < n_params; ++i) {
      double sx = 0.0;
      for (int k = 0; k < n_iter; ++k) {
       sx += AS_VALUE(chain_[k].values_[i]);
      }
      mean_var[i] = sx / n_iter;

      LOG_MEDIUM() << "Total = " << sx << "\n";
      LOG_MEDIUM() << "Mean = " << mean_var[i]  << "\n";

      double sxx = 0.0;
      for (int k = 0; k < n_iter; ++k) {
       sxx += pow(AS_VALUE(chain_[k].values_[i]) - mean_var[i],2);
      }
      double var = sxx / (n_iter - 1);
      temp_covariance(i,i) = var;
      for (int j = 0; j < i; j++) {
        double sxy = 0;
        for (int k = 0; k < n_iter; k++){
          sxy += (AS_VALUE(chain_[k].values_[i]) - mean_var[i]) * (AS_VALUE(chain_[k].values_[j]) - mean_var[j]);
        }
        double cov = (sxy / (n_iter - 1));
        temp_covariance(i,j) = cov;
        temp_covariance(j,i) = cov;
      }
    }

    for (int i = 0; i < n_params; ++i){
      for (int k = 0; k < n_params; ++k){
        LOG_MEDIUM() << "row =  " << i << " " << " col = " << k << " " << temp_covariance(i,k);
      }
    }

    covariance_matrix_lt = temp_covariance;

    // Adjust covariance based on maximum correlations and apply Cholesky decompositon
    BuildCovarianceMatrix();

    LOG_MEDIUM() << "Applying Cholesky decomposition";
    if (!DoCholeskyDecmposition())
      LOG_FATAL() << "Cholesky decomposition failed. Cannot continue MCMC";

    // continue chain
    return;
  }
}

/**
 * Generate new estimate candidates
 */
void IndependenceMetropolis::GenerateNewCandidates() {
  //LOG_MEDIUM() << step_size_;
  if (proposal_distribution_ == PARAM_NORMAL)
    FillMultivariateNormal(step_size_);
  else if (proposal_distribution_ == PARAM_T)
    FillMultivariateT(step_size_);

  // For catching errors.
  //for (unsigned i = 0; i < estimate_count_; ++i) {
  //  cerr << candidates_[i] << " ";
  //}
  //cerr << "\n";

}

/*
 * Check that the candidates are within bounds
*/
bool IndependenceMetropolis::WithinBounds() {
  for (unsigned i = 0; i < estimates_.size(); ++i) {
    if (estimates_[i]->lower_bound() > candidates_[i] || estimates_[i]->upper_bound() < candidates_[i]) {
      LOG_MEDIUM() << "Estimate outside of bounds = " << estimates_[i]->parameter() << " value = " << candidates_[i]
        << " u_b = " << estimates_[i]->upper_bound() << " l_b = " << estimates_[i]->lower_bound();
      return false;
    }
  }

  return true;
}

/*
 * Validate
*/
void IndependenceMetropolis::DoValidate() {
  if (adapt_step_size_.size() == 0)
    adapt_step_size_.assign(1, 1u);

  if (adapt_covariance_matrix_.size() == 0)
    adapt_covariance_matrix_.assign(1, 1u);

  if (adapt_covariance_matrix_.size() > 1)
    LOG_ERROR_P(PARAM_ADAPT_COVARIANCE_AT) << "The covariance matrix can be adapted once only.";

  if (length_ <= 0)
    LOG_ERROR_P(PARAM_LENGTH) << "(" << length_ << ") cannot be less than or equal to 0";

  for (unsigned adapt : adapt_step_size_) {
    if (adapt < 1)
      LOG_ERROR_P(PARAM_ADAPT_STEPSIZE_AT) << "(" << adapt << ") cannot be less than 1";
    if (adapt > length_)
      LOG_ERROR_P(PARAM_ADAPT_STEPSIZE_AT) << "(" << adapt << ") cannot be greater than length (" << length_ << ")";
  }

  if (correlation_method_ != PARAM_CORRELATION && correlation_method_ != PARAM_COVARIANCE && correlation_method_ != PARAM_NONE)
    LOG_ERROR_P(PARAM_COVARIANCE_ADJUSTMENT_METHOD) << "(" << correlation_method_ << ")"
      << " is not supported. Supported values are " << PARAM_CORRELATION << ", " << PARAM_COVARIANCE << ", and " << PARAM_NONE;

  if (proposal_distribution_ != PARAM_T && proposal_distribution_ != PARAM_NORMAL)
    LOG_ERROR_P(PARAM_PROPOSAL_DISTRIBUTION) << "(" << proposal_distribution_ << ")"
      << " is not supported. Supported values are " << PARAM_T << " and " << PARAM_NORMAL;

  if (max_correlation_ <= 0.0 || max_correlation_ > 1.0)
    LOG_ERROR_P(PARAM_MAX_CORRELATION) << "(" << max_correlation_ << ") must be between 0.0 (exclusive) and 1.0 (inclusive)";
  if (df_ <= 0)
    LOG_ERROR_P(PARAM_DF) << "(" << df_ << ") cannot be less or equal to 0";
  if (start_ < 0.0)
    LOG_ERROR_P(PARAM_START) << "(" << start_ << ") cannot be less than 0";
  if (step_size_ < 0.0)
    LOG_ERROR_P(PARAM_STEP_SIZE) << "(" << step_size_ << ") cannot be less than 0.0";
}

/**
 * Build
 */
void IndependenceMetropolis::DoBuild() {
  LOG_MEDIUM() <<"DoBuild MCMC children";

  unsigned active_estimates = 0;
  estimates_ = model_->managers().estimate()->GetIsEstimated();

  for(auto estimate : estimates_) {
    if (!estimate)
      LOG_FATAL() << "Did not find any @estimate blocks. At least one non-fixed estimated parameter is required to run in MCMC mode";
  }

  estimate_count_ = estimates_.size();
  for (Estimate* estimate : estimates_) {
    estimate_labels_.push_back(estimate->label());

    if (estimate->upper_bound() == estimate->lower_bound() || estimate->mcmc_fixed())
      continue;
    active_estimates++;
  }

  if (active_estimates == 0)
    LOG_ERROR() << "The number of active estimates in the MCMC system is 0. At least one non-fixed estimated parameter is required to run in MCMC mode.";

  if (step_size_ == 0.0)
    step_size_ = 2.4 * pow((double)active_estimates, -0.5);
}

/**
 * Execute the MCMC system and build the MCMC chain
 */
void IndependenceMetropolis::DoExecute() {
  candidates_.resize(estimate_count_);
  is_enabled_estimate_.resize(estimate_count_);
  vector<Double> previous_untransformed_candidates = candidates_;

  // Transform any parameters so that candidates are in the same space as the covariance matrix.
  model_->managers().estimate_transformation()->TransformEstimatesForObjectiveFunction();
  for (unsigned i = 0; i < estimate_count_; ++i) {
    candidates_[i] = estimates_[i]->value();

    if (estimates_[i]->lower_bound() == estimates_[i]->upper_bound() || estimates_[i]->mcmc_fixed())
      is_enabled_estimate_[i] = false;
    else
      is_enabled_estimate_[i] = true;
  }

  if (!model_->global_configuration().resume()) {
    LOG_MEDIUM() << "Not resuming";
    BuildCovarianceMatrix();
    LOG_MEDIUM() << "Building Covariance matrix";
    successful_jumps_ = starting_iteration_;
  }

  // Set jumps = starting iteration if it is resuming
  unsigned jumps_since_last_adapt = 1;
  if (model_->global_configuration().resume()) {
    for (unsigned i = 0; i < adapt_step_size_.size(); ++i) {
      if (adapt_step_size_[i] < starting_iteration_) {
        jumps_since_last_adapt = adapt_step_size_[i];
        LOG_MEDIUM() << "Chain last adapted at " << jumps_since_last_adapt;
      }
    }
    jumps_= starting_iteration_;
    jumps_since_adapt_ = jumps_ - jumps_since_last_adapt;

    double temp_success_jumps = (double)jumps_since_adapt_ * acceptance_rate_since_last_adapt_;
    successful_jumps_since_adapt_ = (unsigned)temp_success_jumps;

    //if (!utilities::To<double, unsigned>(temp_success_jumps, successful_jumps_since_adapt_))
    // LOG_ERROR() << "Could not convert " << temp_success_jumps << " to an unsigned integer";

    LOG_FINE() << "jumps = " << jumps_ << "jumps since last adapt " << jumps_since_adapt_
      << " successful jumps since last adapt " << successful_jumps_since_adapt_ << " step size " << step_size_
      << " successful jumps " << successful_jumps_;
  }

  LOG_MEDIUM() << "Applying Cholesky decomposition";
  if (!DoCholeskyDecmposition())
    LOG_FATAL() << "Cholesky decomposition failed. Cannot continue MCMC";

  if (start_ > 0.0) {
    // Take into account any transformations so that when we compare with bounds we are in correct space, when: prior_applies_to_transform true
    GenerateRandomStart();
  }

  for(unsigned i = 0; i < estimate_count_; ++i)
    estimates_[i]->set_value(candidates_[i]);

  /**
   * Get the objective score
   */
  // Do a quick restore so that estimates are in a space the model wants
  model_->managers().estimate_transformation()->RestoreEstimatesFromObjectiveFunction();
  model_->FullIteration();

  // For reporting purposes
  for (unsigned i = 0; i < estimate_count_; ++i) {
    previous_untransformed_candidates[i] = estimates_[i]->value();
  }

  ObjectiveFunction& obj_function = model_->objective_function();
  obj_function.CalculateScore();

  Double score            = AS_VALUE(obj_function.score());
  Double penalty          = AS_VALUE(obj_function.penalties());
  Double prior            = AS_VALUE(obj_function.priors());
  Double likelihood       = AS_VALUE(obj_function.likelihoods());
  Double additional_prior = AS_VALUE(obj_function.additional_priors());
  Double jacobian         = AS_VALUE(obj_function.jacobians());

  /**
   * Store first location
   */
  mcmc::ChainLink new_link;
  new_link.penalty_                       = AS_VALUE(obj_function.penalties());
  new_link.score_                         = AS_VALUE(obj_function.score());
  new_link.prior_                         = AS_VALUE(obj_function.priors());
  new_link.likelihood_                    = AS_VALUE(obj_function.likelihoods());
  new_link.additional_priors_             = AS_VALUE(obj_function.additional_priors());
  new_link.jacobians_                     = AS_VALUE(obj_function.jacobians());
  new_link.step_size_                     = step_size_;
  new_link.values_                        = previous_untransformed_candidates;

  if (!model_->global_configuration().resume()) {
    jumps_++;
    jumps_since_adapt_++;

    new_link.iteration_                   = jumps_;
    new_link.acceptance_rate_             = 0;
    new_link.acceptance_rate_since_adapt_ = 0;

    chain_.push_back(new_link);

    // Print first value
    model_->managers().report()->Execute(State::kIterationComplete);
  } else {
    // resume
    new_link.iteration_                   = jumps_;
    new_link.acceptance_rate_             = acceptance_rate_;
    new_link.acceptance_rate_since_adapt_ = acceptance_rate_since_last_adapt_;

    chain_.push_back(new_link);

    LOG_MEDIUM() << "Resuming MCMC chain with iteration " << jumps_;
  }

  /**
   * Now we start the MCMC process
   */
  utilities::RandomNumberGenerator& rng = utilities::RandomNumberGenerator::Instance();
  LOG_MEDIUM() << "MCMC Starting";
  LOG_MEDIUM() << "Covariance matrix has rows = " << covariance_matrix_.size1() << " and cols = " << covariance_matrix_.size2();
  LOG_MEDIUM() << "Estimate Count: " << estimate_count_;

  vector<Double> previous_candidates = candidates_;

  Double previous_score            = score;
  Double previous_prior            = prior;
  Double previous_likelihood       = likelihood;
  Double previous_penalty          = penalty;
  Double previous_additional_prior = additional_prior;
  Double previous_jacobian         = jacobian;

  do {
    // Check If we need to update the step size
    UpdateStepSize();

    // Check If we need to update the covariance
    UpdateCovarianceMatrix();

    // Generate new candidates
    // Need to make sure estimates are in the correct space.
    model_->managers().estimate_transformation()->TransformEstimatesForObjectiveFunction();
    GenerateNewCandidates();

    // Count the jump
    jumps_++;
    jumps_since_adapt_++;

    // Check candidates are within the bounds.
    if (WithinBounds()) {
      // Trial these Potential candidates.
      for (unsigned i = 0; i < candidates_.size(); ++i)
        estimates_[i]->set_value(candidates_[i]);

      // restore for model run.
      model_->managers().estimate_transformation()->RestoreEstimatesFromObjectiveFunction();

      // Run model with candidate parameters.
      model_->FullIteration();
      // evaluate objective score.
      obj_function.CalculateScore();

      // Store objective information if we accept these will become our current step
      score            = AS_VALUE(obj_function.score());
      penalty          = AS_VALUE(obj_function.penalties());
      prior            = AS_VALUE(obj_function.priors());
      likelihood       = AS_VALUE(obj_function.likelihoods());
      additional_prior = AS_VALUE(obj_function.additional_priors());
      jacobian         = AS_VALUE(obj_function.jacobians());

      Double ratio = 1.0;

      if (score >= previous_score) {
        ratio = exp(previous_score - score);
      }

      // Check if we accept this jump
      if (dc::IsEqual(ratio, 1.0) || rng.uniform() < ratio) {
        LOG_MEDIUM() << "Accept: Possible. Iteration = " << jumps_ << ", score = " << score << " Previous score " << previous_score;
        // Accept this jump
        successful_jumps_++;
        successful_jumps_since_adapt_++;

        // So these become our last step values so save them.
        previous_candidates       = candidates_;
        previous_score            = score;
        previous_prior            = prior;
        previous_likelihood       = likelihood;
        previous_penalty          = penalty;
        previous_additional_prior = additional_prior;
        previous_jacobian         = jacobian;

        // For reporting purposes
        for (unsigned i = 0; i < estimate_count_; ++i) {
          previous_untransformed_candidates[i] = estimates_[i]->value();
        }
      } else {
        // reject this jump reset
        candidates_ = previous_candidates;

        LOG_MEDIUM() << "Reject: Possible. Iteration = " << jumps_ << ", score = " << score << " Previous score " << previous_score;
      }
    } else {
      LOG_MEDIUM() << "Reject: Bounds. Iteration = " << jumps_ << ", score = " << score << " Previous score " << previous_score;
      // Reject this attempt but still record the chain if it lands on a keep
      candidates_ = previous_candidates;
    }

    if (jumps_ % keep_ == 0) {
      // Record the score, and its compontent parts if the successful jump divided by keep has no remainder
      // i.e this proposed candidate is a 'keep' iteration
      mcmc::ChainLink new_link;

      new_link.iteration_                   = jumps_;
      new_link.penalty_                     = previous_penalty;
      new_link.score_                       = previous_score;
      new_link.prior_                       = previous_prior;
      new_link.likelihood_                  = previous_likelihood;
      new_link.additional_priors_           = previous_additional_prior;
      new_link.jacobians_                   = previous_jacobian;
      new_link.acceptance_rate_             = double(successful_jumps_) / double(jumps_);
      new_link.acceptance_rate_since_adapt_ = double(successful_jumps_since_adapt_) / double(jumps_since_adapt_);
      new_link.step_size_                   = step_size_;
      new_link.values_                      = previous_untransformed_candidates;

      chain_.push_back(new_link);

      //LOG_MEDIUM() << "Storing: Successful Jumps " << successful_jumps_ << " Jumps : " << jumps_;
      model_->managers().report()->Execute(State::kIterationComplete);
    }
  } while (jumps_ < length_);
}

} /* namespace mcmcs */
} /* namespace niwa */
