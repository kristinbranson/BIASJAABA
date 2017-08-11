#include "node_map_spin.hpp"
#include "basic_types.hpp"
#include "exception.hpp"
#include <sstream>

namespace bias 
{
    const size_t MAX_BUF_LEN = 512;
    // ----------------------------------------------------------------------------------
    // NodeMap_spin
    // ----------------------------------------------------------------------------------


    // NodeMap_spin - public methods
    // ----------------------------------------------------------------------------------

    NodeMap_spin::NodeMap_spin(spinCamera hCamera)
    { }

    size_t NodeMap_spin::numberOfNodes()
    {
        size_t numNodes = 0;
        spinError err = SPINNAKER_ERR_SUCCESS;
        err = spinNodeMapGetNumNodes(hNodeMap_,&numNodes);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get number of nodes, error=" << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_VENDOR_NAME, ssError.str());
        }
        return numNodes;
    }

    std::vector<std::string> NodeMap_spin::nodeNames()
    {
        std::vector<std::string> nodeNameVec;

        for (size_t i=0; i<numberOfNodes(); i++) 
        { 
            spinNodeHandle hNode = nullptr;
            try
            {
                getNodeHandleByIndex(i, hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }


            std::string nodeName("");
            try
            {
                nodeName = getNodeName(hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }

            nodeNameVec.push_back(nodeName);
        }

        return nodeNameVec;
    }

    std::vector<std::string> NodeMap_spin::nodeNames(spinNodeType nodeType)
    {
        std::vector<std::string> nodeNameVec;

        for (size_t i=0; i<numberOfNodes(); i++) 
        { 
            spinNodeHandle hNode = nullptr;
            try
            {
                getNodeHandleByIndex(i, hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }

            std::string nodeName("");
            try
            {
                nodeName = getNodeName(hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }

            spinNodeType currNodeType;
            try
            {
                currNodeType = getNodeType(hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }

            if (currNodeType == nodeType)
            {
                nodeNameVec.push_back(nodeName);
            }
        }

        return nodeNameVec;


    }


    std::map<std::string, spinNodeType> NodeMap_spin::nodeNameToTypeMap()
    {
        std::map<std::string, spinNodeType> nameToType;
        for (size_t i=0; i<numberOfNodes(); i++) 
        {
            spinNodeHandle hNode = nullptr;
            try
            {
                getNodeHandleByIndex(i, hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }
            std::string nodeName("");
            try
            {
                nodeName = getNodeName(hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }

            spinNodeType nodeType;
            try
            {
                nodeType = getNodeType(hNode);
            }
            catch (RuntimeError &runtimeError)
            {
                continue;
            }
            nameToType[nodeName] = nodeType;
        }
        return nameToType;
    }


    std::string NodeMap_spin::getStringNodeValueByName(std::string nodeName)
    {
        spinNodeHandle hNode = nullptr;
        getNodeHandleByName(nodeName, hNode);
        return getStringNodeValue(hNode);
    }

    std::string NodeMap_spin::getStringNodeValueByIndex(size_t index)
    {
        spinNodeHandle hNode = nullptr;
        getNodeHandleByIndex(index, hNode);
        return getStringNodeValue(hNode);
    }

    // NodeMap_spin - protected methods
    // ----------------------------------------------------------------------------------

    void NodeMap_spin::getNodeHandleByName(std::string nodeName, spinNodeHandle &hNode)
    {
        spinError err = spinNodeMapGetNode(hNodeMap_, nodeName.c_str(), &hNode);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node handle, error=" << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_HANDLE, ssError.str());
        }

    }


    void NodeMap_spin::getNodeHandleByIndex(size_t index, spinNodeHandle &hNode)
    {
        spinError err = spinNodeMapGetNodeByIndex(hNodeMap_, index, &hNode);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node handle, error=" << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_HANDLE, ssError.str());
        }
    }


    spinNodeType NodeMap_spin::getNodeType(spinNodeHandle &hNode)
    {
        spinNodeType nodeType;
        spinError err = spinNodeGetType(hNode, &nodeType);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node type,  error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_TYPE, ssError.str());

        }
        return nodeType;
    }


    std::string NodeMap_spin::getNodeName(spinNodeHandle &hNode)
    {
        char buffer[MAX_BUF_LEN];
        size_t bufferLen = MAX_BUF_LEN;
        spinError err = spinNodeGetName(hNode, buffer, &bufferLen);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node name, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_NAME, ssError.str());
        }
        return std::string(buffer);
    }


    std::string NodeMap_spin::getNodeDisplayName(spinNodeHandle &hNode)
    {
        char buffer[MAX_BUF_LEN];
        size_t bufferLen = MAX_BUF_LEN;
        spinError err = spinNodeGetDisplayName(hNode, buffer, &bufferLen);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node display name, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_DISPLAY_NAME, ssError.str());
        }
        return std::string(buffer);
    }


    std::string NodeMap_spin::getNodeToolTip(spinNodeHandle &hNode)
    {
        char buffer[MAX_BUF_LEN];
        size_t bufferLen = MAX_BUF_LEN;
        spinError err = spinNodeGetToolTip(hNode, buffer, &bufferLen);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node tooltip, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_TOOLTIP, ssError.str());
        }
        return std::string(buffer);
    }


    std::string NodeMap_spin::getNodeDescription(spinNodeHandle &hNode)
    {
        char buffer[MAX_BUF_LEN];
        size_t bufferLen = MAX_BUF_LEN;
        spinError err = spinNodeGetDescription(hNode, buffer, &bufferLen);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node description, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_DESCRIPTION, ssError.str());
        }
        return std::string(buffer);
    }


    bool NodeMap_spin::getNodeAvailability(spinNodeHandle &hNode)
    {
        bool8_t isAvailable = False;
        spinError err = spinNodeIsAvailable(hNode, &isAvailable);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node availability, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_AVAILABLE, ssError.str());
        }
        return (isAvailable == False) ? false : true;
    }


    bool NodeMap_spin::getNodeReadability(spinNodeHandle &hNode)
    {
        bool8_t isReadable = False;
        spinError err = spinNodeIsReadable(hNode, &isReadable);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get node readability, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_NODE_READABLE, ssError.str());
        }
        return (isReadable == False) ? false : true;
    }


    std::string NodeMap_spin::getStringNodeValue(spinNodeHandle &hNode)
    {
        bool isAvailable = getNodeAvailability(hNode);
        if (!isAvailable)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": node is not available";
            throw RuntimeError(ERROR_SPIN_NODE_NOT_AVAILABLE, ssError.str());

        }

        bool isReadable = getNodeReadability(hNode);
        if (!isReadable)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": node is not readable";
            throw RuntimeError(ERROR_SPIN_NODE_NOT_READABLE, ssError.str());

        }

        spinNodeType nodeType = getNodeType(hNode);
        if (nodeType != StringNode)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": incorrect node type";
            throw RuntimeError(ERROR_SPIN_INCORRECT_NODE_TYPE, ssError.str());
        }

        char buffer[MAX_BUF_LEN];
        size_t bufferLen = MAX_BUF_LEN;

        spinError err = spinStringGetValue(hNode,buffer,&bufferLen);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to get string node value, error = " << err;
            throw RuntimeError(ERROR_SPIN_RETRIEVE_STRING_VALUE, ssError.str());
        }

        return std::string(buffer);
    }


    // ----------------------------------------------------------------------------------
    // NodeMapCamera_spin
    // ----------------------------------------------------------------------------------
    

    // NodeMapCamera_spin
    // ----------------------------------------------------------------------------------
    
    NodeMapCamera_spin::NodeMapCamera_spin(spinCamera hCamera) : NodeMap_spin(hCamera)
    {
        // Get camera node map
        spinError err = spinCameraGetNodeMap(hCamera, &hNodeMap_);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to retrieve Spinnaker GenICam node map, error=" << err;
            throw RuntimeError(ERROR_SPIN_GET_CAMERA_NODE_MAP, ssError.str());
        }
    }


    // ----------------------------------------------------------------------------------
    // NodeMapTLDevice_spin
    // ----------------------------------------------------------------------------------



    // NodeMapTLDevice_spin
    // ----------------------------------------------------------------------------------
    NodeMapTLDevice_spin::NodeMapTLDevice_spin(spinCamera hCamera) : NodeMap_spin(hCamera)
    {
        // Get TLDevice node map
        spinError err = spinCameraGetTLDeviceNodeMap(hCamera, &hNodeMap_);
        if (err != SPINNAKER_ERR_SUCCESS)
        {
            std::stringstream ssError;
            ssError << __PRETTY_FUNCTION__;
            ssError << ": unable to retrieve Spinnaker TL device node map, error=" << err;
            throw RuntimeError(ERROR_SPIN_GET_TLDEVICE_NODE_MAP, ssError.str());
        }
    }




} // namespace bias