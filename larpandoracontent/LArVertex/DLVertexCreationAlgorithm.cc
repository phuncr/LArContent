/**
 *  @file   larpandoracontent/DLVertexCreationAlgorithm.cc
 * 
 *  @brief  Implementation of the DLVertexCreation algorithm class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h" 

#include "larpandoracontent/LArHelpers/LArGeometryHelper.h"
#include "larpandoracontent/LArHelpers/LArMCParticleHelper.h"
#include "larpandoracontent/LArHelpers/LArFileHelper.h"

#include "larpandoracontent/LArVertex/DLVertexCreationAlgorithm.h"

using namespace pandora;

namespace lar_content
{

DLVertexCreationAlgorithm::DLVertexCreationAlgorithm() :
    m_outputVertexListName(),
    m_inputClusterListNames(),
    m_filePathEnvironmentVariable("FW_SEARCH_PATH"),
    m_numClusterCaloHitsPar(5),
    m_npixels(128),
    m_lenVec(),
    m_trainingSetMode(false),
    m_trainingLenVecIndex(0),
    m_lenBuffer(10.0),
    m_pModule()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------
DLVertexCreationAlgorithm::~DLVertexCreationAlgorithm()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------
StatusCode DLVertexCreationAlgorithm::Initialize()
{
    if(m_lenVec.empty()) m_lenVec={-1.0f, 50.0f, 40.0f};
    // Load the model file. 
    m_pModule = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    std::vector<std::string> view = {"W", "V", "U"};

    int numModels(m_trainingSetMode?m_trainingLenVecIndex:m_lenVec.size());
    for(int j=0; j<numModels;j++)
    {
        for(int i=0; i<3;i++)
        {
            std::string fileNameString(m_modelFileNamePrefix+std::to_string(j)+view[i]+".pt");
            std::string fullFileNameString(LArFileHelper::FindFileInPath(fileNameString, m_filePathEnvironmentVariable));

            try
            {
                m_pModule[3*j+i] = torch::jit::load(fullFileNameString);
            }
            catch (const c10::Error &e)
            {
                std::cout << "Error loading the PyTorch module" << std::endl;
                return STATUS_CODE_FAILURE;
            }
        }
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------
StatusCode DLVertexCreationAlgorithm::Run()
{
    torch::manual_seed(0);

    const ClusterList *clustersU, *clustersV, *clustersW;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_inputClusterListNames[2], clustersW));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_inputClusterListNames[1], clustersV));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetList(*this, m_inputClusterListNames[0], clustersU));

    std::stringstream ssBuf[6];
    std::string viewW="W", viewV="V", viewU="U";
    CartesianVector positionInput(0.f, 0.f, 0.f); int vertReconCount(0);
    CartesianVector positionW(this->GetDLVertexForView(clustersW, viewW, positionInput, 0, vertReconCount, ssBuf));
    CartesianVector positionV(this->GetDLVertexForView(clustersV, viewV, positionInput, 0, vertReconCount, ssBuf));
    CartesianVector positionU(this->GetDLVertexForView(clustersU, viewU, positionInput, 0, vertReconCount, ssBuf));
    if(m_trainingSetMode && (m_trainingLenVecIndex==0)) 
    {
        if(vertReconCount==(3+m_trainingLenVecIndex*3))
            this->WriteTrainingFiles(ssBuf);
        return STATUS_CODE_SUCCESS;
    }

    CartesianVector position3D(0.f, 0.f, 0.f); float chiSquared(0);
    for(int i=1; i<m_lenVec.size(); i++)
    {
        LArGeometryHelper::MergeThreePositions3D(this->GetPandora(), TPC_VIEW_W, TPC_VIEW_V, TPC_VIEW_U,
                    positionW, positionV, positionU, position3D, chiSquared);

        CartesianVector position3DW(LArGeometryHelper::ProjectPosition(this->GetPandora(), position3D, TPC_VIEW_W));
        CartesianVector position3DV(LArGeometryHelper::ProjectPosition(this->GetPandora(), position3D, TPC_VIEW_V));
        CartesianVector position3DU(LArGeometryHelper::ProjectPosition(this->GetPandora(), position3D, TPC_VIEW_U));

        positionW=this->GetDLVertexForView(clustersW, viewW, position3DW, i, vertReconCount, ssBuf);
        positionV=this->GetDLVertexForView(clustersV, viewV, position3DV, i, vertReconCount, ssBuf);
        positionU=this->GetDLVertexForView(clustersU, viewU, position3DU, i, vertReconCount, ssBuf);
        if(m_trainingSetMode && (m_trainingLenVecIndex==i))
        {
            if(vertReconCount==(3+m_trainingLenVecIndex*3))
                this->WriteTrainingFiles(ssBuf);
            return STATUS_CODE_SUCCESS;
        }
    }

    if(vertReconCount==3*m_lenVec.size())
    {
        LArGeometryHelper::MergeThreePositions3D(this->GetPandora(), TPC_VIEW_W, TPC_VIEW_V, TPC_VIEW_U,
                    positionW, positionV, positionU, position3D, chiSquared);

        const VertexList *pVertexList(nullptr); std::string temporaryListName;
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::CreateTemporaryListAndSetCurrent(*this, pVertexList, temporaryListName));

        PandoraContentApi::Vertex::Parameters parameters;
        parameters.m_position = position3D;
        parameters.m_vertexLabel = VERTEX_INTERACTION;
        parameters.m_vertexType = VERTEX_3D;
        const Vertex *pVertex(nullptr);

        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Vertex::Create(*this, parameters, pVertex));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList<Vertex>(*this, m_outputVertexListName));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentList<Vertex>(*this, m_outputVertexListName));
    }

    return STATUS_CODE_SUCCESS;
}

//---------------------------------------------------------------------------------------------------------------------------------
CartesianVector DLVertexCreationAlgorithm::GetDLVertexForView(const pandora::ClusterList *pClusterList, const std::string &view,
                const CartesianVector &positionInput, const int &lenVecIndex, int &vertReconCount, std::stringstream ssBuf[6]) const
{
    double length(m_lenVec[lenVecIndex]);
    std::vector<double> xVec, zVec, sigma, height;
    int i(0), j(0), l(0), count1(0), count2(0);

    for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
    {
        if( ((*iter)->GetNCaloHits()<m_numClusterCaloHitsPar) && (length<=0)) continue;
        if( ((*iter)->GetNCaloHits()>=m_numClusterCaloHitsPar) ) count2+=(*iter)->GetNCaloHits();
        count1+=(*iter)->GetNCaloHits();
    }
    if(count1==0||count2==0) return(CartesianVector(0.f, 0.f, 0.f));

    xVec.resize(count1, 0.f) ; zVec.resize(count1, 0.f)  ;
    sigma.resize(count1, 0.f); height.resize(count1, 0.f);

    for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
    {
        if( ((*iter)->GetNCaloHits()<m_numClusterCaloHitsPar) && (length<=0)) continue;
        const OrderedCaloHitList &orderedCaloHitList1((*iter)->GetOrderedCaloHitList());
        for (OrderedCaloHitList::const_iterator iter1 = orderedCaloHitList1.begin(), iter1End = orderedCaloHitList1.end(); iter1 != iter1End; ++iter1)
        {
            for (CaloHitList::const_iterator hitIter1 = iter1->second->begin(), hitIter1End = iter1->second->end(); hitIter1 != hitIter1End; ++hitIter1)
            {
                const CartesianVector &positionVector1((*hitIter1)->GetPositionVector());
                xVec[l]=positionVector1.GetX(); zVec[l]=positionVector1.GetZ();
                sigma[l]=(*hitIter1)->GetCellSize1(); height[l]=(*hitIter1)->GetInputEnergy(); l++;
            }
        }
    }

    double minx=0, minz=0, nstepx=0, nstepz=0;
    double zC=0, xC=0;
    std::vector<std::vector<double>> out2dVec(m_npixels, std::vector<double>(m_npixels, 0.f));
    double lengthX(0), lengthZ(0), length1(0);

    if(length<=0)
    {
        minx=(*std::min_element(xVec.begin(),xVec.end()))-m_lenBuffer;
        minz=(*std::min_element(zVec.begin(),zVec.end()))-m_lenBuffer;
        lengthX=(*std::max_element(xVec.begin(),xVec.end())) - (*std::min_element(xVec.begin(),xVec.end()))+2*m_lenBuffer;
        lengthZ=(*std::max_element(zVec.begin(),zVec.end())) - (*std::min_element(zVec.begin(),zVec.end()))+2*m_lenBuffer;
        length1=std::max(lengthX, lengthZ);
        nstepx=length1/m_npixels;
        nstepz=length1/m_npixels;
    }
    else
    {
        minx=positionInput.GetX()-length/2.0;
        minz=positionInput.GetZ()-length/2.0;
        nstepx=length/m_npixels;
        nstepz=length/m_npixels;
    }

    if(length<=0)
    {
        std::vector<double>().swap(xVec);std::vector<double>().swap(zVec);
        std::vector<double>().swap(sigma);std::vector<double>().swap(height); count1=0; l=0;
        for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
        {
            count1+=(*iter)->GetNCaloHits();
        }
        if(count1==0) return(CartesianVector(0.f, 0.f, 0.f));

        xVec.resize(count1, 0.f) ; zVec.resize(count1, 0.f)  ;
        sigma.resize(count1, 0.f); height.resize(count1, 0.f);

        for (ClusterList::const_iterator iter = pClusterList->begin(), iterEnd = pClusterList->end(); iter != iterEnd; ++iter)
        {
            const OrderedCaloHitList &orderedCaloHitList1((*iter)->GetOrderedCaloHitList());
            for (OrderedCaloHitList::const_iterator iter1 = orderedCaloHitList1.begin(), iter1End = orderedCaloHitList1.end(); iter1 != iter1End; ++iter1)
            {
                for (CaloHitList::const_iterator hitIter1 = iter1->second->begin(), hitIter1End = iter1->second->end(); hitIter1 != hitIter1End; ++hitIter1)
                {
                    const CartesianVector &positionVector1((*hitIter1)->GetPositionVector());
                    xVec[l]=positionVector1.GetX(); zVec[l]=positionVector1.GetZ();
                    sigma[l]=(*hitIter1)->GetCellSize1(); height[l]=(*hitIter1)->GetInputEnergy(); l++;
                }
            }
        }
    }

    for(i=0;i<l;i++)
    {
         zC=(int)((zVec[i]-minz)/nstepz); xC=(int)((xVec[i]-minx)/nstepx);
         if(xC>m_npixels||xC<0||zC>m_npixels||zC<0) continue;
         if(zC==m_npixels) zC--;

         for(j=0;j<m_npixels;j++)
         {
             double tempval(0),sigmaZ(0.5),inputvalue1(0),dist(0);
             tempval=height[i]*(0.5*std::erfc(-((minx+(j+1)*nstepx)-xVec[i])/std::sqrt(2*sigma[i]*sigma[i]))
                                   - 0.5*std::erfc(-((minx+j*nstepx)-xVec[i])/std::sqrt(2*sigma[i]*sigma[i])));

             for(int a=m_npixels-1;a>-1;a--)
             {
                 dist=(minz+(a+1)*nstepz)-(zVec[i]+sigmaZ/2.0);

                 if(dist>nstepz)
                     inputvalue1=0;
                 else if(dist<nstepz&&dist>0)
                     inputvalue1=std::min((nstepz-dist),sigmaZ)*tempval;
                 else if(dist<0 && std::fabs(dist)<nstepz)
                     {if(std::fabs(dist)>sigmaZ) inputvalue1=0; else inputvalue1=std::min((sigmaZ-std::fabs(dist)),nstepz)*tempval;}
                 else if(dist<0 && std::fabs(dist)>nstepz)
                     {if(std::fabs(dist)>sigmaZ) inputvalue1=0; else inputvalue1=std::min((sigmaZ-std::fabs(dist)),nstepz)*tempval;}

                 out2dVec[j][a]+=inputvalue1;
             }
         }
    }

    if(m_trainingSetMode && (m_trainingLenVecIndex==lenVecIndex))
    {
        vertReconCount+=(this->CreateTrainingFiles(out2dVec,view,minx,nstepx,minz,nstepz,ssBuf));
        return(CartesianVector(0.f, 0.f, 0.f));
    }
    else
    {
        /*******************************************************************************************/
        /* Use Deep Learning here on the created image to get the 2D vertex coordinate for 1 view. */ 
        CartesianVector pixelPosition(this->DeepLearning(out2dVec,view,lenVecIndex));
        /*******************************************************************************************/

        double recoX(minx+(pixelPosition.GetX())*nstepx);
        double recoZ(minz+(pixelPosition.GetZ())*nstepz);
        CartesianVector position(recoX, 0.f, recoZ);
        vertReconCount+=1;
        return(position);
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
CartesianVector DLVertexCreationAlgorithm::DeepLearning(const std::vector<std::vector<double>> &out2dVec, const std::string &view,
                const int &lenVecIndex) const
{
    /* Get the index for model */
    int index(0);
    if(view=="W") index=3*lenVecIndex+0;
    if(view=="V") index=3*lenVecIndex+1;
    if(view=="U") index=3*lenVecIndex+2;

    /* Convert image to Torch "Tensor" */
    torch::Tensor input = torch::zeros({1, 1, m_npixels, m_npixels}, torch::TensorOptions().dtype(torch::kFloat32));
    auto accessor = input.accessor<float, 4>();

    for(int i=m_npixels-1;i>-1;i--)
        for(int j=0;j<m_npixels;j++)
            accessor[0][0][i][j]=out2dVec[j][i]/100.0;

    std::vector<torch::jit::IValue> inputs;
    inputs.push_back(input);

    /* Use the Model on the "Tensor" */
    at::Tensor output = m_pModule[index]->forward(inputs).toTensor();

    /* Get predicted vertex. */
    auto outputAccessor = output.accessor<float, 2>();
    CartesianVector position(outputAccessor[0][0], 0.f, outputAccessor[0][1]);

    return(position);
}

//------------------------------------------------------------------------------------------------------------------------------------------
int DLVertexCreationAlgorithm::CreateTrainingFiles(const std::vector<std::vector<double>> &out2dVec, const std::string &view,
     const float &minx, const float &nstepx, const float &minz, const float &nstepz, std::stringstream ssBuf[6]) const
{
    const MCParticleList *pMCParticleList(nullptr);
    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentList(*this, pMCParticleList));

    CartesianVector targetVertex(0.f, 0.f, 0.f);
    for (const MCParticle *const pMCParticle : *pMCParticleList)
    {
        if (!LArMCParticleHelper::IsNeutrino(pMCParticle))
            continue;

        targetVertex.SetValues(pMCParticle->GetEndpoint().GetX(), pMCParticle->GetEndpoint().GetY(), 
                                            pMCParticle->GetEndpoint().GetZ());
    }

    int flag(1);
    flag=flag && (((targetVertex.GetX()<362.6620483395 )&&(targetVertex.GetX()>3.24703979450001  )) ||
                  ((targetVertex.GetX()>-362.6620483395)&&(targetVertex.GetX()<-3.24703979450001 )));
    flag=flag &&  ((targetVertex.GetY()<603.92376709   )&&(targetVertex.GetY()>-603.92376709     ));
    flag=flag &&  ((targetVertex.GetZ()<1393.463745117 )&&(targetVertex.GetZ()>-0.876220703000058));
    if(!flag) return(0);

    CartesianVector VertexPosition(0.f, 0.f, 0.f); int index(0);
    if(view=="W") {VertexPosition=(LArGeometryHelper::ProjectPosition(this->GetPandora(), targetVertex, TPC_VIEW_W));index=0;}
    if(view=="V") {VertexPosition=(LArGeometryHelper::ProjectPosition(this->GetPandora(), targetVertex, TPC_VIEW_V));index=1;}
    if(view=="U") {VertexPosition=(LArGeometryHelper::ProjectPosition(this->GetPandora(), targetVertex, TPC_VIEW_U));index=2;}

    double xC=((VertexPosition.GetX()-minx)/nstepx);double zC=((VertexPosition.GetZ()-minz)/nstepz);
    if((int)xC>m_npixels||(int)xC<0||(int)zC>m_npixels||(int)zC<0) return(0);

    for(int i=m_npixels-1;i>-1;i--)
    {
        for(int j=0;j<m_npixels;j++)
        {
                ssBuf[index] << out2dVec[j][i] << " ";
        }
                ssBuf[index] << "\n";
    }
    ssBuf[3+index] << xC << " " << zC << "\n";

    return(1);
}

//------------------------------------------------------------------------------------------------------------------------------------------
void DLVertexCreationAlgorithm::WriteTrainingFiles(std::stringstream ssBuf[6]) const
{
    std::vector<std::string> view = {"W", "V", "U"};
    for(int i=0; i<3;i++)
    {
        std::ofstream outputStream1("data"+view[i]+".txt", std::ios::app);
        std::ofstream outputStream2("labels"+view[i]+".txt", std::ios::app);
        
        outputStream1 << ssBuf[i].rdbuf();
        outputStream2 << ssBuf[3+i].rdbuf();

        outputStream1.close();
        outputStream2.close();
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------
StatusCode DLVertexCreationAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    // Read settings from xml file here
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle,
        "OutputVertexListName", m_outputVertexListName));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadVectorOfValues(xmlHandle,
        "InputClusterListNames", m_inputClusterListNames));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle,
        "ModelFileNamePrefix", m_modelFileNamePrefix));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "FilePathEnvironmentVariable", m_filePathEnvironmentVariable));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "NumClusterCaloHitsPar", m_numClusterCaloHitsPar));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "Npixels", m_npixels));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadVectorOfValues(xmlHandle,
        "LenVec", m_lenVec));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "TrainingSetMode", m_trainingSetMode));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "TrainingLenVecIndex", m_trainingLenVecIndex));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "LenBuffer", m_lenBuffer));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
