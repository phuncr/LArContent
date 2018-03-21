/**
 *  @file   larpandoracontent/LArObjects/MultivariateAnalysisBaseClass.h
 *
 *  @brief  Header file for the lar multivariate analysis base class.
 *
 *  $Log: $
 */
#ifndef LAR_MULTIVARIATE_ANALYSIS_BASE_CLASS_H
#define LAR_MULTIVARIATE_ANALYSIS_BASE_CLASS_H 1

namespace lar_content
{

typedef std::vector<double> DoubleVector;

/**
 *  @brief  MultivariateAnalysisBaseClass class
 */
class MultivariateAnalysisBaseClass
{
public:
    /**
     *  @brief  Classify the set of input features based on the trained model
     *
     *  @param  features the input features
     *
     *  @return the classification 
     */
    virtual bool Classify(const DoubleVector &features) const = 0;

    /**
     *  @brief  Calculate the classification score for a set of input features, based on the trained model
     *
     *  @param  features the input features
     *
     *  @return the classification score
     */
    virtual double CalculateClassificationScore(const DoubleVector &features) const = 0;

    /**
     *  @brief  Calculate the classification probability for a set of input features, based on the trained model
     *
     *  @param  features the input features
     *
     *  @return the classification probability
     */
    virtual double CalculateProbability(const DoubleVector &features) const = 0;
};

} // namespace lar_content

#endif // #ifndef LAR_MULTIVARIATE_ANALYSIS_BASE_CLASS_H