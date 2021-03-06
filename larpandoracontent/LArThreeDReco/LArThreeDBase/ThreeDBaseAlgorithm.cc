/**
 *  @file   larpandoracontent/LArThreeDReco/LArThreeDBase/ThreeDBaseAlgorithm.cc
 *
 *  @brief  Implementation of the three dimension algorithm base class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArHelpers/LArClusterHelper.h"

#include "larpandoracontent/LArObjects/LArOverlapTensor.h"
#include "larpandoracontent/LArObjects/LArShowerOverlapResult.h"
#include "larpandoracontent/LArObjects/LArTrackOverlapResult.h"

#include "larpandoracontent/LArThreeDReco/LArThreeDBase/ThreeDBaseAlgorithm.h"

using namespace pandora;

namespace lar_content
{

template <typename T>
ThreeDBaseAlgorithm<T>::ThreeDBaseAlgorithm() :
    m_pInputClusterListU(NULL),
    m_pInputClusterListV(NULL),
    m_pInputClusterListW(NULL)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
ThreeDBaseAlgorithm<T>::~ThreeDBaseAlgorithm()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
bool ThreeDBaseAlgorithm<T>::CreateThreeDParticles(const ProtoParticleVector &protoParticleVector)
{
    bool particlesMade(false);
    const PfoList *pPfoList = NULL; std::string pfoListName;
    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::CreateTemporaryListAndSetCurrent(*this, pPfoList, pfoListName));

    for (typename ProtoParticleVector::const_iterator iter = protoParticleVector.begin(), iterEnd = protoParticleVector.end(); iter != iterEnd; ++iter)
    {
        PandoraContentApi::ParticleFlowObject::Parameters pfoParameters;
        this->SetPfoParameters(*iter, pfoParameters);

        const ParticleFlowObject *pPfo(NULL);
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ParticleFlowObject::Create(*this, pfoParameters, pPfo));
        particlesMade = true;
    }

    if (!pPfoList->empty())
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList<Pfo>(*this, m_outputPfoListName));
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentList<Pfo>(*this, m_outputPfoListName));
    }

    return particlesMade;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
bool ThreeDBaseAlgorithm<T>::MakeClusterMerges(const ClusterMergeMap &clusterMergeMap)
{
    ClusterSet deletedClusters;

    ClusterList parentClusters;
    for (const auto &mapEntry : clusterMergeMap) parentClusters.push_back(mapEntry.first);
    parentClusters.sort(LArClusterHelper::SortByNHits);

    for (const Cluster *const pParentCluster : parentClusters)
    {
        const HitType hitType(LArClusterHelper::GetClusterHitType(pParentCluster));
        const std::string clusterListName((TPC_VIEW_U == hitType) ? this->GetClusterListNameU() : (TPC_VIEW_V == hitType) ? this->GetClusterListNameV() : this->GetClusterListNameW());

        if (!((TPC_VIEW_U == hitType) || (TPC_VIEW_V == hitType) || (TPC_VIEW_W == hitType)))
            throw StatusCodeException(STATUS_CODE_FAILURE);

        ClusterList daughterClusters(clusterMergeMap.at(pParentCluster));
        daughterClusters.sort(LArClusterHelper::SortByNHits);

        for (const Cluster *const pDaughterCluster : daughterClusters)
        {
            if (deletedClusters.count(pParentCluster) || deletedClusters.count(pDaughterCluster))
                throw StatusCodeException(STATUS_CODE_FAILURE);

            this->UpdateUponDeletion(pDaughterCluster);
            this->UpdateUponDeletion(pParentCluster);
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::MergeAndDeleteClusters(*this, pParentCluster, pDaughterCluster, clusterListName, clusterListName));

            this->UpdateForNewCluster(pParentCluster);
            deletedClusters.insert(pDaughterCluster);
        }
    }

    return !(deletedClusters.empty());
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::UpdateForNewCluster(const Cluster *const pNewCluster)
{
    const HitType hitType(LArClusterHelper::GetClusterHitType(pNewCluster));

    if (!((TPC_VIEW_U == hitType) || (TPC_VIEW_V == hitType) || (TPC_VIEW_W == hitType)))
        throw StatusCodeException(STATUS_CODE_FAILURE);

    ClusterList &clusterList((TPC_VIEW_U == hitType) ? m_clusterListU : (TPC_VIEW_V == hitType) ? m_clusterListV : m_clusterListW);

    if (clusterList.end() != std::find(clusterList.begin(), clusterList.end(), pNewCluster))
        throw StatusCodeException(STATUS_CODE_ALREADY_PRESENT);

    clusterList.push_back(pNewCluster);

    const ClusterList &clusterList1((TPC_VIEW_U == hitType) ? m_clusterListV : m_clusterListU);
    const ClusterList &clusterList2((TPC_VIEW_W == hitType) ? m_clusterListV : m_clusterListW);

    ClusterVector clusterVector1(clusterList1.begin(), clusterList1.end());
    ClusterVector clusterVector2(clusterList2.begin(), clusterList2.end());
    std::sort(clusterVector1.begin(), clusterVector1.end(), LArClusterHelper::SortByNHits);
    std::sort(clusterVector2.begin(), clusterVector2.end(), LArClusterHelper::SortByNHits);

    for (const Cluster *const pCluster1 : clusterVector1)
    {
        for (const Cluster *const pCluster2 : clusterVector2)
        {
            if (TPC_VIEW_U == hitType)
            {
                this->CalculateOverlapResult(pNewCluster, pCluster1, pCluster2);
            }
            else if (TPC_VIEW_V == hitType)
            {
                this->CalculateOverlapResult(pCluster1, pNewCluster, pCluster2);
            }
            else
            {
                this->CalculateOverlapResult(pCluster1, pCluster2, pNewCluster);
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::UpdateUponDeletion(const Cluster *const pDeletedCluster)
{
    ClusterList::iterator iterU = std::find(m_clusterListU.begin(), m_clusterListU.end(), pDeletedCluster);
    ClusterList::iterator iterV = std::find(m_clusterListV.begin(), m_clusterListV.end(), pDeletedCluster);
    ClusterList::iterator iterW = std::find(m_clusterListW.begin(), m_clusterListW.end(), pDeletedCluster);

    if (m_clusterListU.end() != iterU)
        m_clusterListU.erase(iterU);

    if (m_clusterListV.end() != iterV)
        m_clusterListV.erase(iterV);

    if (m_clusterListW.end() != iterW)
        m_clusterListW.erase(iterW);

    m_overlapTensor.RemoveCluster(pDeletedCluster);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::RemoveUnavailableTensorElements()
{
    const typename TensorType::ClusterNavigationMap &navigationMapUV(m_overlapTensor.GetClusterNavigationMapUV());
    const typename TensorType::ClusterNavigationMap &navigationMapVW(m_overlapTensor.GetClusterNavigationMapVW());
    const typename TensorType::ClusterNavigationMap &navigationMapWU(m_overlapTensor.GetClusterNavigationMapWU());
    ClusterList usedClusters;

    for (typename TensorType::ClusterNavigationMap::const_iterator iter = navigationMapUV.begin(), iterEnd = navigationMapUV.end(); iter != iterEnd; ++iter)
    {
        if (!(iter->first->IsAvailable()))
            usedClusters.push_back(iter->first);
    }

    for (typename TensorType::ClusterNavigationMap::const_iterator iter = navigationMapVW.begin(), iterEnd = navigationMapVW.end(); iter != iterEnd; ++iter)
    {
        if (!(iter->first->IsAvailable()))
            usedClusters.push_back(iter->first);
    }

    for (typename TensorType::ClusterNavigationMap::const_iterator iter = navigationMapWU.begin(), iterEnd = navigationMapWU.end(); iter != iterEnd; ++iter)
    {
        if (!(iter->first->IsAvailable()))
            usedClusters.push_back(iter->first);
    }

    usedClusters.sort(LArClusterHelper::SortByNHits);

    for (ClusterList::const_iterator iter = usedClusters.begin(), iterEnd = usedClusters.end(); iter != iterEnd; ++iter)
        this->UpdateUponDeletion(*iter);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::SelectAllInputClusters()
{
    this->SelectInputClusters(m_pInputClusterListU, m_clusterListU);
    this->SelectInputClusters(m_pInputClusterListV, m_clusterListV);
    this->SelectInputClusters(m_pInputClusterListW, m_clusterListW);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::PreparationStep()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::TidyUp()
{
    m_overlapTensor.Clear();

    m_pInputClusterListU = NULL;
    m_pInputClusterListV = NULL;
    m_pInputClusterListW = NULL;

    m_clusterListU.clear();
    m_clusterListV.clear();
    m_clusterListW.clear();
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
StatusCode ThreeDBaseAlgorithm<T>::Run()
{
    try
    {
        PANDORA_THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_INITIALIZED, !=, PandoraContentApi::GetList(*this,
            m_inputClusterListNameU, m_pInputClusterListU));
        PANDORA_THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_INITIALIZED, !=, PandoraContentApi::GetList(*this,
            m_inputClusterListNameV, m_pInputClusterListV));
        PANDORA_THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_INITIALIZED, !=, PandoraContentApi::GetList(*this,
            m_inputClusterListNameW, m_pInputClusterListW));

        if ((NULL == m_pInputClusterListU) || (NULL == m_pInputClusterListV) || (NULL == m_pInputClusterListW))
        {
            if (PandoraContentApi::GetSettings(*this)->ShouldDisplayAlgorithmInfo())
                std::cout << "ThreeDBaseAlgorithm: one or more input cluster lists unavailable." << std::endl;
            throw StatusCodeException(STATUS_CODE_SUCCESS);
        }

        this->SelectAllInputClusters();
        this->PreparationStep();
        this->PerformMainLoop();
        this->ExamineTensor();
        this->TidyUp();
    }
    catch (StatusCodeException &statusCodeException)
    {
        this->TidyUp();

        if (STATUS_CODE_SUCCESS != statusCodeException.GetStatusCode())
            throw statusCodeException;
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDBaseAlgorithm<T>::PerformMainLoop()
{
    ClusterVector clusterVectorU(m_clusterListU.begin(), m_clusterListU.end());
    ClusterVector clusterVectorV(m_clusterListV.begin(), m_clusterListV.end());
    ClusterVector clusterVectorW(m_clusterListW.begin(), m_clusterListW.end());
    std::sort(clusterVectorU.begin(), clusterVectorU.end(), LArClusterHelper::SortByNHits);
    std::sort(clusterVectorV.begin(), clusterVectorV.end(), LArClusterHelper::SortByNHits);
    std::sort(clusterVectorW.begin(), clusterVectorW.end(), LArClusterHelper::SortByNHits);

    for (const Cluster *const pClusterU : clusterVectorU)
    {
        for (const Cluster *const pClusterV : clusterVectorV)
        {
            for (const Cluster *const pClusterW : clusterVectorW)
                this->CalculateOverlapResult(pClusterU, pClusterV, pClusterW);
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
StatusCode ThreeDBaseAlgorithm<T>::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "InputClusterListNameU", m_inputClusterListNameU));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "InputClusterListNameV", m_inputClusterListNameV));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "InputClusterListNameW", m_inputClusterListNameW));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputPfoListName", m_outputPfoListName));

    return STATUS_CODE_SUCCESS;
}

template class ThreeDBaseAlgorithm<float>;
template class ThreeDBaseAlgorithm<TransverseOverlapResult>;
template class ThreeDBaseAlgorithm<LongitudinalOverlapResult>;
template class ThreeDBaseAlgorithm<FragmentOverlapResult>;
template class ThreeDBaseAlgorithm<ShowerOverlapResult>;

} // namespace lar_content
