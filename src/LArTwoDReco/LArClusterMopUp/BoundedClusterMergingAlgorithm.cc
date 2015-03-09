/**
 *  @file   LArContent/src/LArTwoDReco/LArClusterMopUp/BoundedClusterMergingAlgorithm.cc
 * 
 *  @brief  Implementation of the bounded cluster merging algorithm class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "LArHelpers/LArClusterHelper.h"
#include "LArHelpers/LArGeometryHelper.h"

#include "LArTwoDReco/LArClusterMopUp/BoundedClusterMergingAlgorithm.h"

using namespace pandora;

namespace lar_content
{

BoundedClusterMergingAlgorithm::BoundedClusterMergingAlgorithm() :
    m_slidingFitWindow(20),
    m_showerEdgeMultiplier(1.5f),
    m_minBoundedFraction(0.5f)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

void BoundedClusterMergingAlgorithm::ClusterMopUp(const ClusterList &pfoClusters, const ClusterList &remnantClusters,
    const ClusterToListNameMap &clusterToListNameMap) const
{
    ClusterAssociationMap clusterAssociationMap;
    const float slidingFitPitch(LArGeometryHelper::GetWireZPitch(this->GetPandora()));

    for (ClusterList::const_iterator pIter = pfoClusters.begin(), pIterEnd = pfoClusters.end(); pIter != pIterEnd; ++pIter)
    {
        const Cluster *const pPfoCluster(*pIter);
        const TwoDSlidingShowerFitResult fitResult(pPfoCluster, m_slidingFitWindow, slidingFitPitch, m_showerEdgeMultiplier);

        ShowerPositionMap showerPositionMap;
        const XSampling xSampling(fitResult.GetShowerFitResult());
        this->GetShowerPositionMap(fitResult, xSampling, showerPositionMap);

        for (ClusterList::const_iterator rIter = remnantClusters.begin(), rIterEnd = remnantClusters.end(); rIter != rIterEnd; ++rIter)
        {
            const Cluster *const pRemnantCluster(*rIter);
            const float boundedFraction(this->GetBoundedFraction(pRemnantCluster, xSampling, showerPositionMap));

            if (boundedFraction < m_minBoundedFraction)
                continue;

            AssociationDetails &associationDetails(clusterAssociationMap[pRemnantCluster]);

            if (!associationDetails.insert(AssociationDetails::value_type(pPfoCluster, boundedFraction)).second)
                throw StatusCodeException(STATUS_CODE_ALREADY_PRESENT);
        }
    }

    this->MakeClusterMerges(clusterAssociationMap, clusterToListNameMap);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void BoundedClusterMergingAlgorithm::GetShowerPositionMap(const TwoDSlidingShowerFitResult &fitResult, const XSampling &xSampling,
    ShowerPositionMap &showerPositionMap) const
{
    for (int n=0; n <= xSampling.m_nPoints; ++n)
    {
        const float x(xSampling.m_minX + (xSampling.m_maxX - xSampling.m_minX) * static_cast<float>(n) / static_cast<float>(xSampling.m_nPoints));

        FloatVector edgePositions;
        fitResult.GetShowerEdges(x, false, edgePositions);

        if (edgePositions.size() < 2)
            continue;

        std::sort(edgePositions.begin(), edgePositions.end());

        try
        {
            const int xBin(xSampling.GetBin(x));
            showerPositionMap.insert(ShowerPositionMap::value_type(xBin, ShowerExtent(x, edgePositions.front(), edgePositions.back())));
        }
        catch (StatusCodeException &)
        {
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

float BoundedClusterMergingAlgorithm::GetBoundedFraction(const Cluster *const pCluster, const XSampling &xSampling, const ShowerPositionMap &showerPositionMap) const
{
  if (((xSampling.m_maxX - xSampling.m_minX) < std::numeric_limits<float>::epsilon()) || (0 >= xSampling.m_nPoints) || 
      (0 == pCluster->GetNCaloHits()))
    {
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    unsigned int nMatchedHits(0);
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hIter = iter->second->begin(), hIterEnd = iter->second->end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *const pCaloHit = *hIter;
            const float x(pCaloHit->GetPositionVector().GetX());
            const float z(pCaloHit->GetPositionVector().GetZ());

            try
            {
                const int xBin(xSampling.GetBin(x));
            
                ShowerPositionMap::const_iterator positionIter = showerPositionMap.find(xBin);

                if ((showerPositionMap.end() != positionIter) && (z > positionIter->second.GetLowEdgeZ()) && (z < positionIter->second.GetHighEdgeZ()))
                    ++nMatchedHits;
            }
            catch (StatusCodeException &)
            {
            }
        }
    }

    return (static_cast<float>(nMatchedHits) / static_cast<float>(pCluster->GetNCaloHits()));
}

//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

BoundedClusterMergingAlgorithm::XSampling::XSampling(const TwoDSlidingFitResult &fitResult)
{
    fitResult.GetMinAndMaxX(m_minX, m_maxX);

    m_nPoints = 1 + fitResult.GetMaxLayer() - fitResult.GetMinLayer();

    if (((m_maxX - m_minX) < std::numeric_limits<float>::epsilon()) || (0 >= m_nPoints))
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);
}

//------------------------------------------------------------------------------------------------------------------------------------------

int BoundedClusterMergingAlgorithm::XSampling::GetBin(const float x) const
{
    if (((x - m_minX) < -std::numeric_limits<float>::epsilon()) || ((x - m_maxX) > +std::numeric_limits<float>::epsilon()))
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return static_cast<int>(0.5f + static_cast<float>(m_nPoints) * (x - m_minX) / (m_maxX - m_minX));
}

//------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode BoundedClusterMergingAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, 
        "SlidingFitWindow", m_slidingFitWindow));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, 
        "ShowerEdgeMultiplier", m_showerEdgeMultiplier));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, 
        "MinBoundedFraction", m_minBoundedFraction));

    return ClusterMopUpAlgorithm::ReadSettings(xmlHandle);
}

} // namespace lar_content