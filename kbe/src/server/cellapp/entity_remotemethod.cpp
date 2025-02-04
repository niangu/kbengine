/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2018 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cellapp.h"
#include "witness.h"
#include "entity_remotemethod.h"
#include "entitydef/method.h"
#include "helper/profile.h"	
#include "helper/eventhistory_stats.h"
#include "network/bundle.h"
#include "client_lib/client_interface.h"

namespace KBEngine{	

SCRIPT_METHOD_DECLARE_BEGIN(EntityRemoteMethod)
SCRIPT_METHOD_DECLARE_END()

SCRIPT_MEMBER_DECLARE_BEGIN(EntityRemoteMethod)
SCRIPT_MEMBER_DECLARE_END()

SCRIPT_GETSET_DECLARE_BEGIN(EntityRemoteMethod)
SCRIPT_GETSET_DECLARE_END()
SCRIPT_INIT(EntityRemoteMethod, tp_call, 0, 0, 0, 0)	

//-------------------------------------------------------------------------------------
EntityRemoteMethod::EntityRemoteMethod(MethodDescription* methodDescription, 
						EntityCallAbstract* entitycall):
RemoteEntityMethod(methodDescription, entitycall, getScriptType())
{
}

//-------------------------------------------------------------------------------------
EntityRemoteMethod::~EntityRemoteMethod()
{
}

//-------------------------------------------------------------------------------------
PyObject* EntityRemoteMethod::tp_call(PyObject* self, PyObject* args, 
	PyObject* kwds)	
{	
	EntityRemoteMethod* rmethod = static_cast<EntityRemoteMethod*>(self);
	MethodDescription* methodDescription = rmethod->getDescription();
	EntityCallAbstract* entitycall = rmethod->getEntityCall();

	if(!entitycall->isClient())
	{
		return RemoteEntityMethod::tp_call(self, args, kwds);
	}

	Entity* pEntity = Cellapp::getSingleton().findEntity(entitycall->id());
	if(pEntity == NULL || pEntity->pWitness() == NULL)
	{
		//WARNING_MSG(fmt::format("EntityRemoteMethod::callClientMethod: not found entity({}).\n", 
		//	entitycall->id()));

		return RemoteEntityMethod::tp_call(self, args, kwds);
	}

	Network::Channel* pChannel = pEntity->pWitness()->pChannel();
	if(!pChannel)
	{
		PyErr_Format(PyExc_AssertionError, "%s:EntityRemoteMethod(%s)::tp_call: no client, srcEntityID(%d).\n",
			pEntity->scriptName(), methodDescription->getName(), pEntity->id());		
		PyErr_PrintEx(0);
		return RemoteEntityMethod::tp_call(self, args, kwds);
	}
	
	// 如果是调用客户端方法， 我们记录事件并且记录带宽
	if(methodDescription->checkArgs(args))
	{
		Network::Bundle* pBundle = pChannel->createSendBundle();
		entitycall->newCall((*pBundle));

		MemoryStream* mstream = MemoryStream::createPoolObject(OBJECTPOOL_POINT);

		try
		{
			methodDescription->addToStream(mstream, args);
		}
		catch (MemoryStreamWriteOverflow & err)
		{
			ERROR_MSG(fmt::format("EntityRemoteMethod::tp_call: {}::{} {}, error={}!\n",
				pEntity->scriptName(), methodDescription->getName(), pEntity->id(), err.what()));

			MemoryStream::reclaimPoolObject(mstream);
			S_Return;
		}

		if(mstream->wpos() > 0)
			(*pBundle).append(mstream->data(), (int)mstream->wpos());

		if(Network::g_trace_packet > 0)
		{
			if(Network::g_trace_packet_use_logfile)
				DebugHelper::getSingleton().changeLogger("packetlogs");

			DEBUG_MSG(fmt::format("EntityRemoteMethod::tp_call: pushUpdateData: ClientInterface::onRemoteMethodCall({}::{})\n",
				pEntity->scriptName(), methodDescription->getName()));

			switch(Network::g_trace_packet)
			{
			case 1:
				mstream->hexlike();
				break;
			case 2:
				mstream->textlike();
				break;
			default:
				mstream->print_storage();
				break;
			};

			if(Network::g_trace_packet_use_logfile)
				DebugHelper::getSingleton().changeLogger(COMPONENT_NAME_EX(g_componentType));																				
		}

		// 记录这个事件产生的数据量大小
		g_privateClientEventHistoryStats.trackEvent(pEntity->scriptName(), 
			methodDescription->getName(), 
			pBundle->currMsgLength(), 
			"::");
		
		pEntity->pWitness()->sendToClient(ClientInterface::onRemoteMethodCall, pBundle);
		MemoryStream::reclaimPoolObject(mstream);
	}
	
	S_Return;
}	

//-------------------------------------------------------------------------------------
}
