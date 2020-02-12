#include "di_srocs_loop_functions.h"

#include <argos3/plugins/simulator/entities/block_entity.h>
#include <argos3/plugins/robots/builderbot/simulator/builderbot_entity.h>

namespace argos {

   /****************************************/
   /****************************************/

   CDISRoCSLoopFunctions::CDISRoCSLoopFunctions() :
      m_cSpace(CSimulator::GetInstance().GetSpace()),
      m_cArenaRangeX(-1.0,1.0),
      m_cArenaRangeY(-1.0,1.0) {
      /* add a default body node to the block template node */
      TConfigurationNode tBodyNode("body");
      AddChildNode(m_tBlockTemplateNode, tBodyNode);
      /* add a controller node to the block template node */
      TConfigurationNode tControllerNode("controller");
      SetNodeAttribute(tControllerNode, "config", "block");
      AddChildNode(m_tBlockTemplateNode, tControllerNode);
   }

   /****************************************/
   /****************************************/

   void CDISRoCSLoopFunctions::Init(TConfigurationNode& t_tree) {
      /* parse loop function configuration */
      GetNodeAttribute(t_tree, "construction_site_min_corner", m_cConstructionSiteMinCorner);
      GetNodeAttribute(t_tree, "construction_site_max_corner", m_cConstructionSiteMaxCorner);
      GetNodeAttribute(t_tree, "blocks_in_foraging_area", m_unTargetBlocksInForagingArea);
      /* calculate the corners of the arena */
      const CVector3& cArenaSize(m_cSpace.GetArenaSize());
      const CVector3& cArenaCenter(m_cSpace.GetArenaCenter());    
      m_cArenaRangeX.Set(cArenaCenter.GetX() - (cArenaSize.GetX() * 0.5) + 0.1,
                         cArenaCenter.GetX() + (cArenaSize.GetX() * 0.5) - 0.1);
      m_cArenaRangeY.Set(cArenaCenter.GetY() - (cArenaSize.GetY() * 0.5) + 0.1,
                         cArenaCenter.GetY() + (cArenaSize.GetY() * 0.5) - 0.1);
      /* create a map of the builderbot robots */
      using TValueType = std::pair<const std::string, CAny>;
      for(const TValueType& t_robot : m_cSpace.GetEntitiesByType("builderbot")) {
         m_vecRobots.push_back(any_cast<CBuilderBotEntity*>(t_robot.second));
      }
      /* create a map of stigmergic blocks */
      for(const TValueType& t_block : m_cSpace.GetEntitiesByType("block")) {
         m_vecBlocks.push_back(any_cast<CBlockEntity*>(t_block.second));
      }
   }

   /****************************************/
   /****************************************/

   void CDISRoCSLoopFunctions::PreStep() {
      UInt32 unBlocksInConstructionSite = 0;
      UInt32 unBlocksInForagingArea = 0;
      /* loop over all the blocks */
      for(CBlockEntity* pc_block : m_vecBlocks) {
         SAnchor& sAnchor = pc_block->GetEmbodiedEntity().GetOriginAnchor();
         if(sAnchor.Position.GetX() < m_cConstructionSiteMaxCorner.GetX() &&
            sAnchor.Position.GetX() > m_cConstructionSiteMinCorner.GetX() &&
            sAnchor.Position.GetY() < m_cConstructionSiteMaxCorner.GetY() &&
            sAnchor.Position.GetY() > m_cConstructionSiteMinCorner.GetY()) {
            unBlocksInConstructionSite++;
         }
         else {
            unBlocksInForagingArea++;
         }
      }

      if(unBlocksInForagingArea < m_unTargetBlocksInForagingArea) {
         for(;;) {
            /* find a location for a block */
            Real fX = CSimulator::GetInstance().GetRNG()->Uniform(m_cArenaRangeX);
            Real fY = CSimulator::GetInstance().GetRNG()->Uniform(m_cArenaRangeY);
            /* check that the location is not inside the construction site */
            if(fX > m_cConstructionSiteMinCorner.GetX() &&
               fX < m_cConstructionSiteMaxCorner.GetX() &&
               fY > m_cConstructionSiteMinCorner.GetY() &&
               fY < m_cConstructionSiteMaxCorner.GetY()) {
               continue;
            }
            /* do not space items within 25 cm of another robot */
            bool bTooClose = false;
            for(CBuilderBotEntity* pc_robot : m_vecRobots) {
               SAnchor& sAnchor = pc_robot->GetEmbodiedEntity().GetOriginAnchor();
               if(Distance(sAnchor.Position, CVector3(fX, fY, 0.0)) < 0.25) {
                  bTooClose = true;
                  break;
               }
            }
            if(bTooClose) {
               continue;
            }
            /* attempt to place block and check for collisions */
            TConfigurationNode& tBody = GetNode(m_tBlockTemplateNode, "body");
            SetNodeAttribute(m_tBlockTemplateNode, "id", "block" + std::to_string(m_vecBlocks.size()));
            SetNodeAttribute(tBody, "position", CVector3(fX, fY, 0.25));
            //SetNodeAttribute(tBodyNode, "orientation", (*pcOrientationGenerator)(bRetry));
            /* Create and initialized the block */
            CBlockEntity* pcBlock = new CBlockEntity;
            pcBlock->Init(m_tBlockTemplateNode);
            CallEntityOperation<CSpaceOperationAddEntity, CSpace, void>(m_cSpace, *pcBlock);
            /* Check if it's colliding with something */
            if(pcBlock->GetEmbodiedEntity().IsCollidingWithSomething()) {
               CallEntityOperation<CSpaceOperationRemoveEntity, CSpace, void>(m_cSpace, *pcBlock);
               continue;
            }
            /* all checks passed */
            m_vecBlocks.push_back(pcBlock);
            break;
         }
      }

      //std::cerr << "unBlocksInConstructionSite = " << static_cast<int>(unBlocksInConstructionSite) << std::endl;
      //std::cerr << "unBlocksInForagingArea = " << static_cast<int>(unBlocksInForagingArea) << std::endl;

   }

   /****************************************/
   /****************************************/

   REGISTER_LOOP_FUNCTIONS(CDISRoCSLoopFunctions, "di_srocs_loop_functions");

}