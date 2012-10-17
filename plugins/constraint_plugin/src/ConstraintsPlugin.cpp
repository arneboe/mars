/*
 *  Copyright 2012, DFKI GmbH Robotics Innovation Center
 *
 *  This file is part of the MARS simulation framework.
 *
 *  MARS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3
 *  of the License, or (at your option) any later version.
 *
 *  MARS is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with MARS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/**
 * \file ConstraintsPlugin.h
 * \author Lorenz Quack (lorenz.quack@dfki.de)
 * \brief Plugin to create constraints between Nodes.
 *
 * Version 0.1
 */


#include "ConstraintsPlugin.h"
#include "NodeConstraint.h"

#include <mars/interfaces/sim/NodeManagerInterface.h>

#include <QString>
#include <QFileDialog>

namespace mars {
  namespace plugins {
    
    namespace constraints_plugin {

      using cfg_manager::cfgPropertyStruct;
      using cfg_manager::cfgParamInfo;
      using cfg_manager::cfgParamId;

      enum ConstraintAction {
        ACTION_LOAD_DEFS,
        ACTION_SAVE_DEFS,
        ACTION_LOAD,
        ACTION_SAVE,
      };

      ConstraintsPlugin::ConstraintsPlugin(lib_manager::LibManager *theManager) :
        interfaces::MarsPluginTemplate(theManager, "constraints_plugin") {
        control->cfg->registerToCFG(this);
        gui = theManager->getLibraryAs<main_gui::GuiInterface>("main_gui");
      }
                   

      ConstraintsPlugin::~ConstraintsPlugin() {
        for(ConstraintsLookup::iterator it = constraints.begin();
            it != constraints.end(); ++it) {
          cfgParamRemoved(it->first);
        }
        constraints.clear();
      }

      void ConstraintsPlugin::init() {
        gui->addGenericMenuAction("../Constraints/Load Constraint Definitions...", ACTION_LOAD_DEFS, this);
        gui->addGenericMenuAction("../Constraints/Save Constraint Definitions...", ACTION_SAVE_DEFS, this);
        gui->addGenericMenuAction("../Constraints/Load Constraints...", ACTION_LOAD, this);
        gui->addGenericMenuAction("../Constraints/Save Constraints...", ACTION_SAVE, this);
      }

      void ConstraintsPlugin::loadConstraintDefs() {
        QString filename;
        filename = QFileDialog::getOpenFileName(NULL,
                                                "Open Constraint Defs...",
                                                QString(),
                                                "YAML-Files (*.yml *.yaml)");
        if(!filename.isEmpty())
          control->cfg->loadConfig(filename.toStdString().c_str());
      }

      void ConstraintsPlugin::saveConstraintDefs() const {
        QString filename;
        filename = QFileDialog::getSaveFileName(NULL,
                                                "Save Constraint Defs...");
        if(!filename.isEmpty())
          control->cfg->writeConfig(filename.toStdString().c_str(),
                                    "ConstraintDefs");
      }

      void ConstraintsPlugin::loadConstraints() {
        QString filename;
        filename = QFileDialog::getOpenFileName(NULL,
                                                "Open Constraints...",
                                                QString(),
                                                "YAML-Files (*.yml *.yaml)");
        if(!filename.isEmpty())
          control->cfg->loadConfig(filename.toStdString().c_str());
      }

      void ConstraintsPlugin::saveConstraints() const {
        QString filename;
        filename = QFileDialog::getSaveFileName(NULL,
                                                "Save Constraints...",
                                                QString(),
                                                "YAML-Files (*.yml *.yaml)");
        if(!filename.isEmpty())
          control->cfg->writeConfig(filename.toStdString().c_str(),
                                    "Constraints");
      }

      void ConstraintsPlugin::cfgUpdateProperty(cfg_manager::cfgPropertyStruct property) {
        // remove param
        cfg_manager::cfgParamInfo constraintDefInfo, constraintInfo;
        constraintDefInfo = control->cfg->getParamInfo(property.paramId);
        constraintInfo = control->cfg->getParamInfo("Constraints", 
                                                    constraintDefInfo.name);
        ConstraintsLookup::iterator it = constraints.find(constraintInfo.id);
        if(it == constraints.end()) {
          LOG_WARN("ConstraintsPlugin::cfgUpdateProperty: constraint not found");
          return;
        }
        for(ConstraintsContainer::iterator it2 = it->second.begin();
            it2 != it->second.end(); ++it2) {
          control->cfg->unregisterFromParam(constraintInfo.id, *it2);
          delete (*it2);
        }
        it->second.clear();
        // recreate param
        std::string s;
        control->cfg->getPropertyValue(constraintDefInfo.id, "value", &s);
        parseConstraintFromString(constraintDefInfo.name, s);
      }

      void ConstraintsPlugin::cfgParamCreated(cfg_manager::cfgParamId id) {
        cfg_manager::cfgParamInfo info = control->cfg->getParamInfo(id);
        if(info.group != "ConstraintDefs")
          return;
        if(info.type != cfg_manager::stringParam) {
          LOG_ERROR("ContraintsPlugin: expected string property! got %d instead",
                    info.type);
          return;
        }
        control->cfg->registerToParam(id, this);
    
        std::string s;
        control->cfg->getPropertyValue(id, "value", &s);

        parseConstraintFromString(info.name, s);
      }
  
      void ConstraintsPlugin::parseConstraintFromString(const std::string &name,
                                                        const std::string &s) {
        // get constraint type
        size_t pos1=0, pos2;
        pos2 = s.find_first_of("#");
        if(s.substr(pos1, pos2) == "NODE") {
          parseNodeConstraints(name, s.substr(pos2+1));
        } else {
          LOG_ERROR("ConstraintsPlugin: Malformed constraint string.");
          return;
        }
      }

      double ConstraintsPlugin::getNodeAttribute(interfaces::NodeId nodeId,
                                                 AttributeType attr) {
        switch(attr) {
        case ATTRIBUTE_POSITION_X:
          return control->nodes->getPosition(nodeId).x();
        case ATTRIBUTE_POSITION_Y:
          return control->nodes->getPosition(nodeId).y();
        case ATTRIBUTE_POSITION_Z:
          return control->nodes->getPosition(nodeId).z();
        case ATTRIBUTE_SIZE_X:
          {
            interfaces::NodeData n = control->nodes->getFullNode(nodeId);
            return n.ext.x();
          }
        case ATTRIBUTE_SIZE_Y:
          {
            interfaces::NodeData n = control->nodes->getFullNode(nodeId);
            return n.ext.y();
          }
        case ATTRIBUTE_SIZE_Z:
          {
            interfaces::NodeData n = control->nodes->getFullNode(nodeId);
            return n.ext.z();
          }
        default:
          LOG_ERROR("ConstraintsPlugin: unsupported AttributeType: %d", attr);
          return 0;
        }
      }

      void ConstraintsPlugin::parseNodeConstraints(const std::string &paramName, 
                                                   const std::string &s) {
        std::string nodeName, nodeAttr;
        interfaces::NodeId nodeId = 0;
        AttributeType attr = ATTRIBUTE_UNDEFINED;
        ParseResult result = PARSE_SUCCESS;
        double factor, initialValue;
        size_t pos1 = 0;

        // create new Property to control the constraint
        cfg_manager::cfgPropertyStruct newProp;
        newProp = control->cfg->getOrCreateProperty("Constraints", paramName, 
                                                    (double)0.);
        // first entry is the initial nodeName.nodeAttr
        result = parseIdentifier(s, &pos1, &nodeId, &attr, NULL);
        if(result != PARSE_SUCCESS)
          return;
        initialValue = getNodeAttribute(nodeId, attr);
        control->cfg->setPropertyValue("Constraints", paramName, "value",
                                       initialValue);
        // add constraint for initial Node with factor 1
        BaseConstraint *c = new NodeConstraint(control, nodeId, attr,
                                               1, initialValue);
        constraints[newProp.paramId].push_back(c);
        control->cfg->registerToParam(newProp.paramId, c);

        while(result == PARSE_SUCCESS) {
          // next there is a list of NodeName.NodeAttr#factor
          result = parseIdentifier(s, &pos1, &nodeId, &attr, &factor);
          if((result != PARSE_SUCCESS) && (result != PARSE_SUCCESS_EOS)) {
            continue;
          }
          BaseConstraint *c = new NodeConstraint(control, nodeId, attr, factor, 
                                                 initialValue);
          constraints[newProp.paramId].push_back(c);
          control->cfg->registerToParam(newProp.paramId, c);
        }
      }

      ParseResult ConstraintsPlugin::parseIdentifier(const std::string &s, 
                                                     size_t *pos,
                                                     interfaces::NodeId *nodeId,
                                                     AttributeType *attr,
                                                     double *factor) {
        // expect the format "nodeName.nodeAttr#factor" where the factor is optional
        size_t startPos=*pos, splitPos, endPos;
        // find end of identifier
        endPos = s.find('#', startPos);
        // find the nodeAttribute. start from rear because nodeName may contain "."
        splitPos = s.rfind('.', endPos);
        std::string nodeName = s.substr(startPos, splitPos - startPos);
        std::string nodeAttr = s.substr(splitPos+1, endPos - splitPos - 1);
    
        if(factor) {
          if(endPos == std::string::npos) {
            *factor = 0;
            LOG_WARN("ConstraintsPlugin::parseIdentifier: factor is missing");
          } else {
            startPos = endPos + 1;
            endPos = s.find('#', startPos);
            *factor = strtod(s.substr(startPos, endPos - startPos).c_str(), NULL);
          }
        }

        // get the actual values from the strings and write the results back
        *nodeId = control->nodes->getID(nodeName);
        *attr = parseAttribute(nodeAttr);
        *pos = endPos + 1;
    
        // do error checking
        if(*nodeId == 0) {
          LOG_ERROR("Invalid NodeName \"%s\" in NodeConstraint: \"%s\"",
                    nodeName.c_str(), s.c_str());
          return PARSE_ERROR_NODEID;
        }
        if(*attr == ATTRIBUTE_UNDEFINED) {
          LOG_ERROR("Invalid NodeAttribute \"%s\" in NodeConstraint: \"%s\"",
                    nodeAttr.c_str(), s.c_str());
          return PARSE_ERROR_NODEATTR;
        }
        // signal whether we reached the End-Of-String or not
        if(endPos == std::string::npos)
          return PARSE_SUCCESS_EOS;
        else
          return PARSE_SUCCESS;
      }

      AttributeType ConstraintsPlugin::parseAttribute(const std::string &s) {
        if(s == "posX")
          return ATTRIBUTE_POSITION_X;
        else if(s == "posY")
          return ATTRIBUTE_POSITION_Y;
        else if(s == "posZ")
          return ATTRIBUTE_POSITION_Z;
        /*
          else if(s == "rotX")
          return ATTRIBUTE_ROTATION_X;
          else if(s == "rotY")
          return ATTRIBUTE_ROTATION_Y;
          else if(s == "rotZ")
          return ATTRIBUTE_ROTATION_Z;
        */
        else if(s == "sizeX")
          return ATTRIBUTE_SIZE_X;
        else if(s == "sizeY")
          return ATTRIBUTE_SIZE_Y;
        else if(s == "sizeZ")
          return ATTRIBUTE_SIZE_Z;
        else {
          LOG_ERROR("unsupported Node attribute in constraint string.");
          return ATTRIBUTE_UNDEFINED;
        }
      }

      void ConstraintsPlugin::cfgParamRemoved(cfg_manager::cfgParamId id) {
        ConstraintsLookup::iterator it = constraints.find(id);
        if(it == constraints.end())
          return;
        for(ConstraintsContainer::iterator it2 = it->second.begin();
            it2 != it->second.end(); ++it2) {
          control->cfg->unregisterFromParam(id, *it2);
          delete (*it2);
        }
        it->second.clear();
      }

      void ConstraintsPlugin::reset() {
    
      }

      void ConstraintsPlugin::update(interfaces::sReal time_ms) {

        // control->motors->setMotorValue(id, value);
      }

      void ConstraintsPlugin::menuAction(int action, bool checked) {
        switch(action) {
        case ACTION_LOAD_DEFS:
          loadConstraintDefs();
          break;
        case ACTION_SAVE_DEFS:
          saveConstraintDefs();
          break;
        case ACTION_LOAD:
          loadConstraints();
          break;
        case ACTION_SAVE:
          saveConstraints();
          break;
        default:
          LOG_WARN("received unknown menu action callback: %d", action);
          break;
        }
      }


    } // end of namespace constraints_plugin
  } // end of namespace plugins
} // end of namespace mars

DESTROY_LIB(mars::plugins::constraints_plugin::ConstraintsPlugin);
CREATE_LIB(mars::plugins::constraints_plugin::ConstraintsPlugin);
