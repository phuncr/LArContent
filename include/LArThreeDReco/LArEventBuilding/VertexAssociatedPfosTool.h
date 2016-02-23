/**
 *  @file   LArContent/include/LArThreeDReco/LArEventBuilding/VertexAssociatedPfosTool.h
 * 
 *  @brief  Header file for the vertex associated pfos tool class.
 * 
 *  $Log: $
 */
#ifndef LAR_VERTEX_ASSOCIATED_PFOS_TOOL_H
#define LAR_VERTEX_ASSOCIATED_PFOS_TOOL_H 1

#include "LArThreeDReco/LArEventBuilding/NeutrinoHierarchyAlgorithm.h"

namespace lar_content
{

/**
 *  @brief  VertexAssociatedPfosTool class
 */
class VertexAssociatedPfosTool : public PfoRelationTool
{
public:
    /**
     *  @brief  Factory class for instantiating algorithm tool
     */
    class Factory : public pandora::AlgorithmToolFactory
    {
    public:
        pandora::AlgorithmTool *CreateAlgorithmTool() const;
    };

    /**
     *  @brief  Default constructor
     */
    VertexAssociatedPfosTool();

    void Run(NeutrinoHierarchyAlgorithm *const pAlgorithm, const pandora::Vertex *const pNeutrinoVertex, NeutrinoHierarchyAlgorithm::PfoInfoMap &pfoInfoMap);

private:
    pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle);

    float       m_minVertexLongitudinalDistance;        ///< Vertex association check: min longitudinal distance cut
    float       m_maxVertexLongitudinalDistance;        ///< Vertex association check: max longitudinal distance cut
    float       m_maxVertexTransverseDistance;          ///< Vertex association check: max transverse distance cut
    float       m_vertexAngularAllowance;               ///< Vertex association check: pointing angular allowance in degrees
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline pandora::AlgorithmTool *VertexAssociatedPfosTool::Factory::CreateAlgorithmTool() const
{
    return new VertexAssociatedPfosTool();
}

} // namespace lar_content

#endif // #ifndef LAR_VERTEX_ASSOCIATED_PFOS_TOOL_H