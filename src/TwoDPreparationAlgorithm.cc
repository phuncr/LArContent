/**
 *  @file   TwoDPreparationAlgorithm.cc
 * 
 *  @brief  Implementation of the transverse clustering algorithm class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "TwoDPreparationAlgorithm.h"

#include "LArVertexHelper.h"

using namespace pandora;

namespace lar
{

StatusCode TwoDPreparationAlgorithm::Run()
{   

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentCaloHitList(*this, m_caloHitListName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentClusterList(*this, m_clusterListName));

    LArVertexHelper::SetCurrentVertex(m_vertexName);

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode TwoDPreparationAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "ClusterListName", m_clusterListName));
    
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "CaloHitListName", m_caloHitListName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "VertexName", m_vertexName));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar