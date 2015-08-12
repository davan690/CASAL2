/**
 * @file VonBertalanffy.Test.cpp
 * @author  Scott Rasmussen (scott.rasmussen@zaita.com)
 * @date 19/08/2013
 * @section LICENSE
 *
 * Copyright NIWA Science �2013 - www.niwa.co.nz
 *
 */
#ifdef TESTMODE

// Headers
#include "VonBertalanffy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <boost/lexical_cast.hpp>

#include "TestResources/MockClasses/Managers.h"
#include "TestResources/MockClasses/Model.h"

// namespaces
namespace niwa {
namespace agelengths {
using ::testing::Return;
using ::testing::ReturnRef;

// classes
class MockTimeStepManager : public timesteps::Manager {
public:
  unsigned time_step_index_ = 0;
  unsigned current_time_step() const override final { return time_step_index_; }
};

class MockVonBertalanffy : public VonBertalanffy {
public:
  MockVonBertalanffy() { };
  MockVonBertalanffy(ModelPtr model, Double linf, Double k, Double t0, bool by_length,
      Double cv_first, Double cv_last, vector<Double> time_step_proportions) : VonBertalanffy(model) {
    linf_ = linf;
    k_ = k;
    t0_ = t0;
    by_length_ = by_length;
    cv_first_ = cv_first;
    cv_last_ = cv_last;
    time_step_proportions_ = time_step_proportions;
  }

  // mocking protected method
  void MockCummulativeNormal(Double mu, Double cv, vector<Double>& vprop_in_length, vector<Double> length_bins, string distribution, bool plus_grp) {
    this->CummulativeNormal(mu, cv, vprop_in_length, length_bins, distribution, plus_grp);
  }
};


/**
 * Test the cumulative normal function that calculates probability of a being in a length bin at a known age
 * This test is for the normal distribution
 */
TEST(AgeLengths, VonBertalanffy_CummulativeNormal) {
  Double mu = 35.49858;
  Double cv = 0.1;
  vector<Double> vprop_in_length;
  vector<Double> length_bins = {0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 ,32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
  string distribution = "normal";
  bool plus_grp = 1;

  MockVonBertalanffy von_bertalanffy;
  von_bertalanffy.MockCummulativeNormal(mu, cv, vprop_in_length, length_bins, distribution,  plus_grp);

  vector<Double> expected = {3.8713535710499514e-009, 1.5960216925847703e-008, 7.422358561104403e-008, 3.1901955588331532e-007, 1.2672619864595447e-006, 4.6525401673491729e-006,
      1.5786604316003761e-005, 4.9506445653380027e-005,0.00014348551812060073, 0.00038434913282614502,0.00095150900849361175, 0.0021770396325317964, 0.0046034492460040877,
      0.0089962477651120976, 0.016247952527619458, 0.027120294871666895, 0.041835938013406682, 0.059643893860880981, 0.078586144395677904, 0.095695106434901311,0.10769489117184539,
      0.11201216057229546,0.10767078378048922,0.095652266453105428,0.078533378916068819, 0.059590504250288112, 0.041789131783027234 , 0.027083887651074501, 0.01622250783484136,
      0.0089801483958439343, 0.0045941822881506722, 0.002172170763177772, 0.00094916847523229819, 0.00059778133048304927};

  ASSERT_EQ(expected.size(), vprop_in_length.size());
  for (unsigned i = 0; i < expected.size(); ++i) {
    EXPECT_DOUBLE_EQ(expected[i], vprop_in_length[i]) << " with i = " << i;
  }
}

/**
 * Test the Cumulative normal function when the distribution is specified as "lognormal" with no plus group
 */
TEST(AgeLengths, VonBertalanffy_CummulativeNormal_2) {
  Double mu = 35.49858;
  Double cv = 0.1;
  vector<Double> vprop_in_length;
  vector<Double> length_bins = {0, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 ,32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
  string distribution = "lognormal";
  bool plus_grp = 0;

  MockVonBertalanffy von_bertalanffy;
  von_bertalanffy.MockCummulativeNormal(mu, cv, vprop_in_length, length_bins, distribution,  plus_grp);

  vector<Double> expected = {0, 9.9920072216264089e-016,1.1390888232654106e-013, 6.907807659217724e-012, 2.4863089365112501e-010, 5.6808661108576075e-009, 8.7191919018181352e-008,
      9.4269457673323842e-007, 7.4745056608538363e-006, 4.4982380957292456e-005, 0.00021163731992057677, 0.00079862796125962365, 0.0024715534075264722, 0.0063962867724943751,0.01408161729231916,
      0.026773528172936767, 0.044555539731829574,  0.065676687795628297, 0.086665248035340148,0.1033532304575121, 0.11234199264093081, 0.11215706824927596, 0.10355520403577334,
      0.088978990579101747, 0.071552403002032916, 0.054126600808507175, 0.038696543948670059, 0.026257468168220055, 0.016976053665368585, 0.010494572238876954, 0.0062237099943515117,
      0.0035513001369547048,0.0019551097231892411 };

  ASSERT_EQ(expected.size(), vprop_in_length.size());
  for (unsigned i = 0; i < expected.size(); ++i) {
    EXPECT_DOUBLE_EQ(expected[i], vprop_in_length[i]) << " with i = " << i;
  }
}

/**
 * Test the DoAgeLengthConversion() so that we know we are applying the right probabilities to the right part of the partition
 */
TEST(AgeLengths, VonBertalanffy_DoAgeLengthConversion) {
  MockTimeStepManager time_step_manager;
  time_step_manager.time_step_index_ = 1;

  MockManagers mock_managers;
  EXPECT_CALL(mock_managers, time_step()).WillRepeatedly(ReturnRef(time_step_manager));

  std::shared_ptr<MockModel> model = std::shared_ptr<MockModel>(new MockModel);
  EXPECT_CALL(*model.get(), min_age()).WillRepeatedly(Return(5));
  EXPECT_CALL(*model.get(), max_age()).WillRepeatedly(Return(10));
  EXPECT_CALL(*model.get(), age_spread()).WillRepeatedly(Return(6));
  EXPECT_CALL(*model.get(), age_plus()).WillRepeatedly(Return(true));
  EXPECT_CALL(*model.get(), managers()).WillRepeatedly(ReturnRef(mock_managers));

  shared_ptr<partition::Category> male = shared_ptr<partition::Category>(new partition::Category());
  male->min_age_ = 5;
  male->max_age_ = 10;
  male->data_.assign(0.0, (male->max_age_ - male->min_age_) + 1);

  for (unsigned age = male->min_age_; age <= male->max_age_; ++age)
    male->mean_length_per_[age] = age * 1.0;

  MockVonBertalanffy von_bertalanffy(model, 70, 0.034, -6, true, 0.0, 0.0, {1.0});
  von_bertalanffy.DoAgeToLengthConversion(male, {1.0, 3.0, 5.0, 7.0});

  //Run through ages and length bins to see if conversion correct
//      EXPECT_DOUBLE_EQ(60000.0 , male->.age_length_matrix_[age][bin]) << " where age = " << age << " where class_bin = " << bin;
//      EXPECT_DOUBLE_EQ(60000.0 , male->.age_length_matrix_[age][bin]) << " where age = " << age << " where class_bin = " << bin;
//      EXPECT_DOUBLE_EQ(60000.0 , male->.age_length_matrix_[age][bin]) << " where age = " << age << " where class_bin = " << bin;
}

} /* namespace agelength */
} /* namespace niwa */
#endif
