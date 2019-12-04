/**
 * \file curve_abc.h
 * \brief interface for a Curve of arbitrary dimension.
 * \author Steve T.
 * \version 0.1
 * \date 06/17/2013
 *
 * Interface for a curve
 */

#ifndef _STRUCT_CURVE_ABC
#define _STRUCT_CURVE_ABC

#include "MathDefs.h"
#include "serialization/archive.hpp"
#include "serialization/eigen-matrix.hpp"
#include <boost/serialization/shared_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

#include <functional>

namespace curves {
/// \struct curve_abc.
/// \brief Represents a curve of dimension Dim.
/// If value of parameter Safe is false, no verification is made on the evaluation of the curve.
template <typename Time = double, typename Numeric = Time, bool Safe = false,
          typename Point = Eigen::Matrix<Numeric, Eigen::Dynamic, 1>, typename Point_derivate = Point>
struct curve_abc : std::unary_function<Time, Point>, public serialization::Serializable {
  typedef Point point_t;
  typedef Point_derivate point_derivate_t;
  typedef Time time_t;
  typedef curve_abc<Time, Numeric, Safe, point_t,point_derivate_t> curve_t; // parent class
  typedef boost::shared_ptr<curve_t> curve_ptr_t;

  /* Constructors - destructors */
 public:
  /// \brief Constructor.
  curve_abc() {}

  /// \brief Destructor.
  virtual ~curve_abc() {}
  /* Constructors - destructors */

  /*Operations*/
  ///  \brief Evaluation of the cubic spline at time t.
  ///  \param t : time when to evaluate the spine
  ///  \return \f$x(t)\f$, point corresponding on curve at time t.
  virtual point_t operator()(const time_t t) const = 0;


  ///  \brief Compute the derived curve at order N.
  ///  \param order : order of derivative.
  ///  \return \f$\frac{d^Nx(t)}{dt^N}\f$ derivative order N of the curve.
  virtual curve_t* compute_derivate(const std::size_t order) const = 0;

  /// \brief Evaluate the derivative of order N of curve at time t.
  /// \param t : time when to evaluate the spline.
  /// \param order : order of derivative.
  /// \return \f$\frac{d^Nx(t)}{dt^N}\f$, point corresponding on derivative curve of order N at time t.
  virtual point_derivate_t derivate(const time_t t, const std::size_t order) const = 0;

  /**
   * @brief isApprox check if other and *this are equals, given a precision treshold.
   * This test is done by discretizing, it should be re-implemented in the child class to check exactly
   * all the members.
   * @param other the other curve to check
   * @param order the order up to which the derivatives of the curves are checked for equality
   * @param prec the precision treshold, default Eigen::NumTraits<Numeric>::dummy_precision()
   * @return true is the two curves are approximately equals
   */
  virtual bool isApprox(const curve_t& other, const Numeric prec = Eigen::NumTraits<Numeric>::dummy_precision(),const size_t order = 5) const{
    bool equal = (min() == other.min())
               && (max() == other.max())
               && (dim() == other.dim());
    if(!equal){
      return false;
    }
    // check the value along the two curves
    Numeric t = min();
    while(t<= max()){
      if(!(*this)(t).isApprox(other(t),prec)){
        return false;
      }
      t += 0.01; // FIXME : define this step somewhere ??
    }
    //  check if the derivatives are equals
    for(size_t n = 1 ; n <= order ; ++n){
      t = min();
      while(t<= max()){
        if(!derivate(t,n).isApprox(other.derivate(t,n),prec)){
          return false;
        }
        t += 0.01; // FIXME : define this step somewhere ??
      }
    }
    return true;
  }

  virtual bool operator==(const curve_t& other) const {
    return isApprox(other);
  }

  virtual bool operator!=(const curve_t& other) const {
    return !(*this == other);
  }

  /*Operations*/

  /*Helpers*/
  /// \brief Get dimension of curve.
  /// \return dimension of curve.
  virtual std::size_t dim() const = 0;
  /// \brief Get the minimum time for which the curve is defined.
  /// \return \f$t_{min}\f$, lower bound of time range.
  virtual time_t min() const = 0;
  /// \brief Get the maximum time for which the curve is defined.
  /// \return \f$t_{max}\f$, upper bound of time range.
  virtual time_t max() const = 0;
  /// \brief Get the degree of the curve.
  /// \return \f$degree\f$, the degree of the curve.
  virtual std::size_t  degree() const =0;

  std::pair<time_t, time_t> timeRange() { return std::make_pair(min(), max()); }
  /*Helpers*/

  // Serialization of the class
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& /*ar*/, const unsigned int version) {
    if (version) {
      // Do something depending on version ?
    }
  }
};
BOOST_SERIALIZATION_ASSUME_ABSTRACT(curve_abc)
}  // namespace curves
#endif  //_STRUCT_CURVE_ABC
