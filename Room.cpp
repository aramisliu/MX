#include "Room.h"
#include "Game.h"
#include "RedisManager.h"

#include <vector>
#include <algorithm>

namespace Adoter
{

/////////////////////////////////////////////////////
//房间
/////////////////////////////////////////////////////
void Room::EnterRoom(std::shared_ptr<Player> player)
{
	if (!player || IsFull()) return;

	_players.emplace(player->GetID(), player); //进入房间

	auto common_prop = player->MutableCommonProp();

	BroadCast(common_prop, player->GetID());
}

/*
void Room::LeaveRoom(std::shared_ptr<Player> player)
{
	if (!player) return;

	_players.erase(player->GetID()); //玩家退出房间
}
*/

std::shared_ptr<Player> Room::GetHoster()
{
	if (_players.size() <= 0) return nullptr;

	return _players.begin()->second; //房间里面的一个人就是房主
}

bool Room::IsHoster(int64_t player_id)
{
	auto host = GetHoster();
	if (!host) return false;

	return host->GetID() == player_id;
}

std::shared_ptr<Player> Room::GetPlayer(int64_t player_id)
{
	auto it = _players.find(player_id);
	if (it == _players.end()) return nullptr;

	return it->second;
}

bool Room::HasPlayer(int64_t player_id)
{
	return _players.find(player_id) != _players.end();
}

void Room::OnPlayerOperate(std::shared_ptr<Player> player, pb::Message* message)
{
	if (!player) return;
	
	auto game_operate = dynamic_cast<Asset::GameOperation*>(message);
	if (!game_operate) return;
			
	BroadCast(game_operate); //广播玩家操作
	
	switch(game_operate->oper_type())
	{
		case Asset::GAME_OPER_TYPE_START: //开始游戏：其实是个准备
		{
			if (!CanStarGame()) return;

			auto game = std::make_shared<Game>();

			game->Init(); //洗牌

			game->Start(_players); //开始游戏
		}
		break;

		case Asset::GAME_OPER_TYPE_LEAVE: //离开游戏
		{
			_players.erase(player->GetID()); //玩家退出房间
		}
		break;
		
		case Asset::GAME_OPER_TYPE_KICKOUT: //踢人
		{
			_players.erase(game_operate->destination_player_id()); //玩家退出房间
		}
		break;
		
		default:
		{
		}
		break;
	}
}

void Room::BroadCast(pb::Message* message, int64_t exclude_player_id)
{
	if (!message) return;

	for (auto player : _players)
	{
		if (exclude_player_id == player.second->GetID()) continue;

		player.second->SendProtocol(message);
	}
}

void Room::OnCreated() 
{ 
	RoomInstance.OnCreateRoom(shared_from_this()); 
}
	
bool Room::CanStarGame()
{
	for (auto player : _players)
	{
		if (!player.second->IsReady()) return false; //需要所有玩家都是准备状态
	}

	return true;
}

/////////////////////////////////////////////////////
//房间通用管理类
/////////////////////////////////////////////////////
std::shared_ptr<Room> RoomManager::Get(int64_t room_id)
{
	auto it = _rooms.find(room_id);
	if (it == _rooms.end()) return nullptr;
	return it->second;
}
	
bool RoomManager::CheckPassword(int64_t room_id, std::string password)
{
	auto room = Get(room_id);
	if (!room) return false;

	return true;
}

int64_t RoomManager::CreateRoom()
{
	std::shared_ptr<Redis> redis = std::make_shared<Redis>();
	int64_t room_id = redis->CreateRoom();
	return room_id;
}

void RoomManager::OnCreateRoom(std::shared_ptr<Room> room)
{
	if (_rooms.find(room->GetID()) != _rooms.end()) return;

	_rooms.emplace(room->GetID(), room);
}

std::shared_ptr<Room> RoomManager::GetAvailableRoom()
{
	std::lock_guard<std::mutex> lock(_no_password_mutex);

	if (_no_password_rooms.size())
	{
		return _no_password_rooms.begin()->second; //取一个未满的房间
	}

	return nullptr;
}

}
