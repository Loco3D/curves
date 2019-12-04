/**
 * \file polynomial.h
 * \brief Definition of a cubic spline.
 * \author Steve T.
 * \version 0.1
 * \date 06/17/2013
 *
 * This file contains definitions for the polynomial struct.
 * It allows the creation and evaluation of natural
 * smooth splines of arbitrary dimension and order
 */

#ifndef _STRUCT_POLYNOMIAL
#define _STRUCT_POLYNOMIAL

#include "MathDefs.h"

#include "curve_abc.h"

#include <iostream>
#include <algorithm>
#include <functional>
#include <stdexcept>

namespace curves {
/// \class polynomial.
/// \brief Represents a polynomial of an arbitrary order defined on the interval
/// \f$[t_{min}, t_{max}]\f$. It follows the equation :<br>
/// \f$ x(t) = a + b(t - t_{min}) + ... + d(t - t_{min})^N \f$<br>
/// where N is the order and \f$ t \in [t_{min}, t_{max}] \f$.
///
template <typename Time = double, typename Numeric = Time, bool Safe = false,
          typename Point = Eigen::Matrix<Numeric, Eigen::Dynamic, 1>,
          typename T_Point = std::vector<Point, Eigen::aligned_allocator<Point> > >
struct polynomial : public curve_abc<Time, Numeric, Safe, Point> {
  typedef Point point_t;
  typedef T_Point t_point_t;
  typedef Time time_t;
  typedef Numeric num_t;
  typedef curve_abc<Time, Numeric, Safe, Point> curve_abc_t;
  typedef Eigen::MatrixXd coeff_t;
  typedef Eigen::Ref<coeff_t> coeff_t_ref;
  typedef polynomial<Time, Numeric, Safe, Point, T_Point> polynomial_t;
  typedef typename curve_abc_t::curve_ptr_t curve_ptr_t;

  /* Constructors - destructors */
 public:
  /// \brief Empty constructor. Curve obtained this way can not perform other class functions.
  ///
  polynomial() : curve_abc_t(), dim_(0), T_min_(0), T_max_(0) {}

  /// \brief Constructor.
  /// \param coefficients : a reference to an Eigen matrix where each column is a coefficient,
  /// from the zero order coefficient, up to the highest order. Spline order is given
  /// by the number of the columns -1.
  /// \param min  : LOWER bound on interval definition of the curve.
  /// \param max  : UPPER bound on interval definition of the curve.
  polynomial(const coeff_t& coefficients, const time_t min, const time_t max)
      : curve_abc_t(),
        dim_(coefficients.rows()),
        coefficients_(coefficients),
        degree_(coefficients.cols() - 1),
        T_min_(min),
        T_max_(max) {
    safe_check();
  }

  /// \brief Constructor
  /// \param coefficients : a container containing all coefficients of the spline, starting
  ///  with the zero order coefficient, up to the highest order. Spline order is given
  ///  by the size of the coefficients.
  /// \param min  : LOWER bound on interval definition of the spline.
  /// \param max  : UPPER bound on interval definition of the spline.
  polynomial(const T_Point& coefficients, const time_t min, const time_t max)
      : curve_abc_t(),
        dim_(coefficients.begin()->size()),
        coefficients_(init_coeffs(coefficients.begin(), coefficients.end())),
        degree_(coefficients_.cols() - 1),
        T_min_(min),
        T_max_(max) {
    safe_check();
  }

  /// \brief Constructor.
  /// \param zeroOrderCoefficient : an iterator pointing to the first element of a structure containing the
  /// coefficients
  ///  it corresponds to the zero degree coefficient.
  /// \param out   : an iterator pointing to the last element of a structure ofcoefficients.
  /// \param min   : LOWER bound on interval definition of the spline.
  /// \param max   : UPPER bound on interval definition of the spline.
  template <typename In>
  polynomial(In zeroOrderCoefficient, In out, const time_t min, const time_t max)
      : curve_abc_t(),
        dim_(zeroOrderCoefficient->size()),
        coefficients_(init_coeffs(zeroOrderCoefficient, out)),
        degree_(coefficients_.cols() - 1),
        T_min_(min),
        T_max_(max) {
    safe_check();
  }

  ///
  /// \brief Constructor from boundary condition with C0 : create a polynomial that connect exactly init and end (order
  /// 1) \param init the initial point of the curve \param end the final point of the curve \param min   : LOWER bound
  /// on interval definition of the spline. \param max   : UPPER bound on interval definition of the spline.
  ///
  polynomial(const Point& init, const Point& end, const time_t min, const time_t max)
      : dim_(init.size()), degree_(1), T_min_(min), T_max_(max) {
    if (init.size() != end.size()) throw std::invalid_argument("init and end points must have the same dimensions.");
    t_point_t coeffs;
    coeffs.push_back(init);
    coeffs.push_back((end - init) / (max - min));
    coefficients_ = init_coeffs(coeffs.begin(), coeffs.end());
    safe_check();
  }

  ///
  /// \brief Constructor from boundary condition with C1 :
  /// create a polynomial that connect exactly init and end and thier first order derivatives(order 3)
  /// \param init the initial point of the curve
  /// \param d_init the initial value of the derivative of the curve
  /// \param end the final point of the curve
  /// \param d_end the final value of the derivative of the curve
  /// \param min   : LOWER bound on interval definition of the spline.
  /// \param max   : UPPER bound on interval definition of the spline.
  ///
  polynomial(const Point& init, const Point& d_init, const Point& end, const Point& d_end, const time_t min,
             const time_t max)
      : dim_(init.size()), degree_(3), T_min_(min), T_max_(max) {
    if (init.size() != end.size()) throw std::invalid_argument("init and end points must have the same dimensions.");
    if (init.size() != d_init.size())
      throw std::invalid_argument("init and d_init points must have the same dimensions.");
    if (init.size() != d_end.size())
      throw std::invalid_argument("init and d_end points must have the same dimensions.");
    /* the coefficients [c0 c1 c2 c3] are found by solving the following system of equation
    (found from the boundary conditions) :
    [1  0  0   0   ]   [c0]   [ init ]
    [1  T  T^2 T^3 ] x [c1] = [ end  ]
    [0  1  0   0   ]   [c2]   [d_init]
    [0  1  2T  3T^2]   [c3]   [d_end ]
    */
    double T = max - min;
    Eigen::Matrix<double, 4, 4> m;
    m << 1., 0, 0, 0, 1., T, T * T, T * T * T, 0, 1., 0, 0, 0, 1., 2. * T, 3. * T * T;
    Eigen::Matrix<double, 4, 4> m_inv = m.inverse();
    Eigen::Matrix<double, 4, 1> bc;                    // boundary condition vector
    coefficients_ = coeff_t::Zero(dim_, degree_ + 1);  // init coefficient matrix with the right size
    for (size_t i = 0; i < dim_; ++i) {                // for each dimension, solve the boundary condition problem :
      bc[0] = init[i];
      bc[1] = end[i];
      bc[2] = d_init[i];
      bc[3] = d_end[i];
      coefficients_.row(i) = (m_inv * bc).transpose();
    }
    safe_check();
  }

  ///
  /// \brief Constructor from boundary condition with C2 :
  /// create a polynomial that connect exactly init and end and thier first and second order derivatives(order 5)
  /// \param init the initial point of the curve
  /// \param d_init the initial value of the derivative of the curve
  /// \param d_init the initial value of the second derivative of the curve
  /// \param end the final point of the curve
  /// \param d_end the final value of the derivative of the curve
  /// \param d_end the final value of the second derivative of the curve
  /// \param min   : LOWER bound on interval definition of the spline.
  /// \param max   : UPPER bound on interval definition of the spline.
  ///
  polynomial(const Point& init, const Point& d_init, const Point& dd_init, const Point& end, const Point& d_end,
             const Point& dd_end, const time_t min, const time_t max)
      : dim_(init.size()), degree_(5), T_min_(min), T_max_(max) {
    if (init.size() != end.size()) throw std::invalid_argument("init and end points must have the same dimensions.");
    if (init.size() != d_init.size())
      throw std::invalid_argument("init and d_init points must have the same dimensions.");
    if (init.size() != d_end.size())
      throw std::invalid_argument("init and d_end points must have the same dimensions.");
    if (init.size() != dd_init.size())
      throw std::invalid_argument("init and dd_init points must have the same dimensions.");
    if (init.size() != dd_end.size())
      throw std::invalid_argument("init and dd_end points must have the same dimensions.");
    /* the coefficients [c0 c1 c2 c3 c4 c5] are found by solving the following system of equation
    (found from the boundary conditions) :
    [1  0  0   0    0     0    ]   [c0]   [ init  ]
    [1  T  T^2 T^3  T^4   T^5  ]   [c1]   [ end   ]
    [0  1  0   0    0     0    ]   [c2]   [d_init ]
    [0  1  2T  3T^2 4T^3  5T^4 ] x [c3] = [d_end  ]
    [0  0  2   0    0     0    ]   [c4]   [dd_init]
    [0  0  2   6T   12T^2 20T^3]   [c5]   [dd_end ]
    */
    double T = max - min;
    Eigen::Matrix<double, 6, 6> m;
    m << 1., 0, 0, 0, 0, 0, 1., T, T * T, pow(T, 3), pow(T, 4), pow(T, 5), 0, 1., 0, 0, 0, 0, 0, 1., 2. * T,
        3. * T * T, 4. * pow(T, 3), 5. * pow(T, 4), 0, 0, 2, 0, 0, 0, 0, 0, 2, 6. * T, 12. * T * T, 20. * pow(T, 3);
    Eigen::Matrix<double, 6, 6> m_inv = m.inverse();
    Eigen::Matrix<double, 6, 1> bc;                    // boundary condition vector
    coefficients_ = coeff_t::Zero(dim_, degree_ + 1);  // init coefficient matrix with the right size
    for (size_t i = 0; i < dim_; ++i) {                // for each dimension, solve the boundary condition problem :
      bc[0] = init[i];
      bc[1] = end[i];
      bc[2] = d_init[i];
      bc[3] = d_end[i];
      bc[4] = dd_init[i];
      bc[5] = dd_end[i];
      coefficients_.row(i) = (m_inv * bc).transpose();
    }
    safe_check();
  }

  /// \brief Destructor
  ~polynomial() {
    // NOTHING
  }

  polynomial(const polynomial& other)
      : dim_(other.dim_),
        coefficients_(other.coefficients_),
        degree_(other.degree_),
        T_min_(other.T_min_),
        T_max_(other.T_max_) {}

  // polynomial& operator=(const polynomial& other);

 private:
  void safe_check() {
    if (Safe) {
      if (T_min_ > T_max_) {
        throw std::invalid_argument("Tmin should be inferior to Tmax");
      }
      if (coefficients_.cols() != int(degree_ + 1)) {
        throw std::runtime_error("Spline order and coefficients do not match");
      }
    }
  }

  /* Constructors - destructors */

  /*Operations*/
 public:
  ///  \brief Evaluation of the cubic spline at time t using horner's scheme.
  ///  \param t : time when to evaluate the spline.
  ///  \return \f$x(t)\f$ point corresponding on spline at time t.
  virtual point_t operator()(const time_t t) const {
    check_if_not_empty();
    if ((t < T_min_ || t > T_max_) && Safe) {
      throw std::invalid_argument(
          "error in polynomial : time t to evaluate should be in range [Tmin, Tmax] of the curve");
    }
    time_t const dt(t - T_min_);
    point_t h = coefficients_.col(degree_);
    for (int i = (int)(degree_ - 1); i >= 0; i--) {
      h = dt * h + coefficients_.col(i);
    }
    return h;
  }

  /**
   * @brief isApprox check if other and *this are equals, given a precision treshold.
   * This test is done by discretizing, it should be re-implemented in the child class to check exactly
   * all the members.
   * @param other the other curve to check
   * @param order the order up to which the derivatives of the curves are checked for equality
   * @param prec the precision treshold, default Eigen::NumTraits<Numeric>::dummy_precision()
   * @return true is the two curves are approximately equals
   */
  virtual bool isApprox(const polynomial_t& other, const Numeric prec = Eigen::NumTraits<Numeric>::dummy_precision(),const size_t order = 5) const{
    //std::cout<<"is approx in polynomial called."<<std::endl;
    (void)order; // silent warning, order is not used in this class.
    return T_min_ == other.min()
        && T_max_ == other.max()
        && dim_ == other.dim()
        && degree_ == other.degree()
        && coefficients_.isApprox(other.coefficients_,prec);
  }

  virtual bool operator==(const polynomial_t& other) const {
    return isApprox(other);
  }

  virtual bool operator!=(const polynomial_t& other) const {
    return !(*this == other);
  }

  virtual bool operator==(const curve_abc_t& other) const {
    return curve_abc_t::isApprox(other);
  }

  virtual bool operator!=(const curve_abc_t& other) const {
    return !curve_abc_t::isApprox(other);
  }


  ///  \brief Evaluation of the derivative of order N of spline at time t.
  ///  \param t : the time when to evaluate the spline.
  ///  \param order : order of derivative.
  ///  \return \f$\frac{d^Nx(t)}{dt^N}\f$ point corresponding on derivative spline at time t.
  virtual point_t derivate(const time_t t, const std::size_t order) const {
    check_if_not_empty();
    if ((t < T_min_ || t > T_max_) && Safe) {
      throw std::invalid_argument(
          "error in polynomial : time t to evaluate derivative should be in range [Tmin, Tmax] of the curve");
    }
    time_t const dt(t - T_min_);
    time_t cdt(1);
    point_t currentPoint_ = point_t::Zero(dim_);
    for (int i = (int)(order); i < (int)(degree_ + 1); ++i, cdt *= dt) {
      currentPoint_ += cdt * coefficients_.col(i) * fact(i, order);
    }
    return currentPoint_;
  }

  polynomial_t* compute_derivate(const std::size_t order) const {
    check_if_not_empty();
    if (order == 0) {
      return new polynomial_t(*this);
    }
    coeff_t coeff_derivated = deriv_coeff(coefficients_);
    polynomial_t deriv(coeff_derivated, T_min_, T_max_);
    return deriv.compute_derivate(order - 1);

  }

  Eigen::MatrixXd coeff() const { return coefficients_; }

  point_t coeffAtDegree(const std::size_t degree) const {
    point_t res;
    if (degree <= degree_) {
      res = coefficients_.col(degree);
    }
    return res;
  }

 private:
  num_t fact(const std::size_t n, const std::size_t order) const {
    num_t res(1);
    for (std::size_t i = 0; i < std::size_t(order); ++i) {
      res *= (num_t)(n - i);
    }
    return res;
  }

  coeff_t deriv_coeff(coeff_t coeff) const {
    if (coeff.cols() == 1)  // only the constant part is left, fill with 0
      return coeff_t::Zero(coeff.rows(), 1);
    coeff_t coeff_derivated(coeff.rows(), coeff.cols() - 1);
    for (std::size_t i = 0; i < std::size_t(coeff_derivated.cols()); i++) {
      coeff_derivated.col(i) = coeff.col(i + 1) * (num_t)(i + 1);
    }
    return coeff_derivated;
  }

  void check_if_not_empty() const {
    if (coefficients_.size() == 0) {
      throw std::runtime_error("Error in polynomial : there is no coefficients set / did you use empty constructor ?");
    }
  }
  /*Operations*/

 public:
  /*Helpers*/
  /// \brief Get dimension of curve.
  /// \return dimension of curve.
  std::size_t virtual dim() const { return dim_; };
  /// \brief Get the minimum time for which the curve is defined
  /// \return \f$t_{min}\f$ lower bound of time range.
  num_t virtual min() const { return T_min_; }
  /// \brief Get the maximum time for which the curve is defined.
  /// \return \f$t_{max}\f$ upper bound of time range.
  num_t virtual max() const { return T_max_; }
  /// \brief Get the degree of the curve.
  /// \return \f$degree\f$, the degree of the curve.
  virtual std::size_t  degree() const {return degree_;}
  /*Helpers*/

  /*Attributes*/
  std::size_t dim_;       // const
  coeff_t coefficients_;  // const
  std::size_t degree_;    // const
  time_t T_min_, T_max_;  // const
                          /*Attributes*/

 private:
  template <typename In>
  coeff_t init_coeffs(In zeroOrderCoefficient, In highestOrderCoefficient) {
    std::size_t size = std::distance(zeroOrderCoefficient, highestOrderCoefficient);
    coeff_t res = coeff_t(dim_, size);
    int i = 0;
    for (In cit = zeroOrderCoefficient; cit != highestOrderCoefficient; ++cit, ++i) {
      res.col(i) = *cit;
    }
    return res;
  }

 public:
  // Serialization of the class
  friend class boost::serialization::access;

  template <class Archive>
  void serialize(Archive& ar, const unsigned int version) {
    if (version) {
      // Do something depending on version ?
    }
    ar& BOOST_SERIALIZATION_BASE_OBJECT_NVP(curve_abc_t);
    ar& boost::serialization::make_nvp("dim", dim_);
    ar& boost::serialization::make_nvp("coefficients", coefficients_);
    ar& boost::serialization::make_nvp("dim", dim_);
    ar& boost::serialization::make_nvp("degree", degree_);
    ar& boost::serialization::make_nvp("T_min", T_min_);
    ar& boost::serialization::make_nvp("T_max", T_max_);
  }

};  // class polynomial

}  // namespace curves
#endif  //_STRUCT_POLYNOMIAL
