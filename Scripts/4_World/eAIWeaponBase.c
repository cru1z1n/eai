WeaponEventBase CreateWeaponEventFromContextAI (int packedType, DayZPlayer player, Magazine magazine)
{
	WeaponEventID eventID = packedType >> 16;
	WeaponEvents animEvent = packedType & 0x0000ffff;
	WeaponEventBase b = WeaponEventFactory(eventID, animEvent, player, magazine);
	//b.ReadFromContext(ctx); // currently does nothing in the bI code anyways
	return b;
}

bool DayZPlayerInventory_OnEventForRemoteWeaponAI (int packedType, DayZPlayer player, Magazine magazine)
{
	PlayerBase pb = PlayerBase.Cast(player);
	
	if (!pb) {
		Error("DayZPlayerInventory_OnEventForRemoteWeaponAI Callback for null PlayerBase! I am giving up, some inventory will be out of sync!");
		return false;
	}
	
	if (!pb.GetDayZPlayerInventory()) {
		Error("DayZPlayerInventory_OnEventForRemoteWeaponAI Callback for " + pb + " has a broken inventory reference!");
		return false;
	}
	
	if (pb.GetDayZPlayerInventory().GetEntityInHands())
	{
		Weapon_Base wpn = Weapon_Base.Cast(pb.GetDayZPlayerInventory().GetEntityInHands());
		if (wpn)
		{
			WeaponEventBase e = CreateWeaponEventFromContextAI(packedType, player, magazine);
			if (pb && e)
			{
				pb.GetWeaponManager().SetRunning(true);
	
				fsmDebugSpam("[wpnfsm] " + Object.GetDebugName(wpn) + " recv event from remote: created event=" + e);
				if (e.GetEventID() == WeaponEventID.HUMANCOMMAND_ACTION_ABORTED)
				{
					wpn.ProcessWeaponAbortEvent(e);
				}
				else
				{
					wpn.ProcessWeaponEvent(e);
				}
				pb.GetWeaponManager().SetRunning(false);
			}
		}
		else
			Error("OnEventForRemoteWeapon - entity in hands, but not weapon. item=" + pb.GetDayZPlayerInventory().GetEntityInHands());
	}
	else
		Error("OnEventForRemoteWeapon - no entity in hands");
	return true;
}

class SceneGraphPoint : ScriptedEntity {
	void SceneGraphPoint(IEntity parent, vector pos) {}
};

modded class Weapon_Base {
	
	// For raycasting bullets in the navmesh
	ref PGFilter pgFilter = new PGFilter();
	
	vector whereIAmAimedAt = "0 0 0";
	//array<Object> thingsICouldBeAimedAt = new array<Object>();
	
	// These two particles are set relative to this weapon when a SendBullet event is registered.
	// The location can be polled on the postframe, when SendBullet is true.
	Object p_front, p_back;	
	//bool SendBullet = false;
	Object toIgnore;
	
	// @params begin_point  The global space point where the raycast will start (front of the muzzle)
	// @param back 			The global space point behind begin_point, usually the back of the barrel
	void BallisticsPostFrame(vector begin_point = "0 0 0", vector back = "0 0 0") {
		//if (SendBullet) {
			//SendBullet = false;
			Print("Postframe reached!...");
		
			Object ignore = toIgnore;

			// this was only required for the serverside way I was trying to do this
			//begin_point = p_front.GetPosition();
			//back = p_back.GetPosition();

			
			// Get ballistics info
			float ammoDamage;
			string ammoTypeName;
			GetCartridgeInfo(GetCurrentMuzzle(), ammoDamage, ammoTypeName);
			
			// Get geometry info
			
			//vector begin_point = p_front.GetPosition();
			//vector back = p_back.GetPosition();
			
			vector aim_point = begin_point - back;

			vector end_point = (500*aim_point) + begin_point;
			
			// Use these to get  an idea of the direction for the raycast
			//GetRPCManager().SendRPC("eAI", "DebugParticle", new Param2<vector, vector>(back, vector.Zero));
			//GetRPCManager().SendRPC("eAI", "DebugParticle", new Param2<vector, vector>(begin_point, vector.Zero));
			//GetRPCManager().SendRPC("eAI", "DebugParticle", new Param2<vector, vector>(begin_point + aim_point , vector.Zero));
			
			
			Print("Muzzle pos: " + begin_point.ToString() + " dir-pos: " + (end_point-begin_point).ToString());
			
			// Prep Raycast
			Object hitObject;
			vector hitPosition, hitNormal;
			float hitFraction;
			int contact_component = 0;
			DayZPhysics.RayCastBullet(begin_point, end_point, hit_mask, this, hitObject, hitPosition, hitNormal, hitFraction);
			//DayZPhysics.RaycastRV(begin_point, aim_point, hitPosition, hitNormal, contact_component, null, null, null, false, false, ObjIntersectFire);
			
			GetRPCManager().SendRPC("eAI", "DebugParticle", new Param2<vector, vector>(hitPosition, vector.Zero));
			
			Print("Raycast hitObject: " + hitObject.ToString() + " hitPosition-pos: " + (hitPosition-begin_point).ToString() + " hitNormal: " + hitNormal.ToString() + " hitFraction " + hitFraction.ToString());
			
			//if (hitPosition && hitNormal) {
			//	Particle p = Particle.PlayInWorld(ParticleList.DEBUG_DOT, hitPosition);
			//	p.SetOrientation(hitNormal);
				
			//}
			
			// So here is an interesting bug... hitObject is always still null even if the raycast succeeded
			// If it succeded then hitPosition, hitNormal, and hitFraction will be accurate
			if (hitFraction > 0.00001) {
				// this could be useful in the future
				//ref array<Object> nearest_objects = new array<Object>;
				//ref array<CargoBase> proxy_cargos = new array<CargoBase>;
				//GetGame().GetObjectsAtPosition ( hitPosition, 1.0, nearest_objects, proxy_cargos );		
													
				array<Object> objects = new array<Object>();
				ref array<CargoBase> proxyCargos = new array<CargoBase>();
				Object closest = null;
				float dist = 1000000.0;
				float testDist;
			
				GetGame().GetObjectsAtPosition3D(hitPosition, 1.5, objects, proxyCargos);
				
				Print(objects);
			
				// not necessary since the ai aren't shooting themselves anymore?
				/*for (int i = 0; i < objects.Count(); i++)
					if (objects[i] == ignore)
						objects.Remove(i);*/
			
				for (int j = 0; j < objects.Count(); j++) {
					if (DayZInfected.Cast(objects[j]) || Man.Cast(objects[j])) {
						testDist = vector.Distance(objects[j].GetPosition(), hitPosition);
						if (testDist < dist) {
							closest = objects[j];
							dist = testDist;
						}
					}
				}
			
				// BUG: hitGround is sometimes still false even when we hit the top of an object.
				
				// As a quick workaround, do a raycast 5cm down from hitPosition
				// If we hit something other than object within 5cm, then we know we have hit the ground and we should not damage "closest."
				vector groundCheckDelta = hitPosition + "0 -0.05 0";
				vector groundCheckContactPos, groundCheckContactDir;
				int contactComponent;
			
				
				int allowFlags = 0;
				allowFlags |= PGPolyFlags.ALL;
				allowFlags |= PGPolyFlags.WALK;
				pgFilter.SetFlags(allowFlags, 0, 0);
				bool hitAnObject = GetGame().GetWorld().GetAIWorld().RaycastNavMesh(hitPosition, groundCheckDelta, pgFilter, groundCheckContactPos, groundCheckContactDir);
				GetRPCManager().SendRPC("eAI", "DebugParticle", new Param2<vector, vector>(groundCheckContactPos, vector.Zero));

				//DayZPhysics.RaycastRV(hitPosition, groundCheckDelta, groundCheckContactPos, groundCheckContactDir, contactComponent, null, null, closest);
				//bool hitGround = (vector.Distance(groundCheckDelta, groundCheckContactPos) > 0.01);
				
				Print("hitEnemy = " + closest.ToString());
				Print("Did we hit an inanimate object? = " + hitAnObject.ToString());
				//Print("hitGround = " + hitGround.ToString());
				
				if (closest && !hitAnObject)// && !hitGround)
					closest.ProcessDirectDamage(DT_FIRE_ARM, null, "Torso", ammoTypeName, closest.WorldToModel(hitPosition), 1.0);
			//}
		}
	}
	
	/**@fn	ProcessWeaponEvent
	 * @brief	weapon's fsm handling of events
	 * @NOTE: warning: ProcessWeaponEvent can be called only within DayZPlayer::HandleWeapons (or ::CommandHandler)
	 **/
	override bool ProcessWeaponEvent (WeaponEventBase e)
	{
		if (GetGame().IsServer() && PlayerBase.Cast(e.m_player).isAI()) {
			// Write the ctx that would normally be sent to the server... note we need to skip writing INPUT_UDT_WEAPON_REMOTE_EVENT
			// since this would normally be Read() and stripped away by the server before calling OnEventForRemoteWeapon
			
			// also wait, I think everyone is meant to execute this
			ScriptRemoteInputUserData ctx = new ScriptRemoteInputUserData;
			Print("Sending weapon event for " + e.GetEventID().ToString() + " player:" + e.m_player.ToString() + " mag:" + e.m_magazine.ToString());
			GetRPCManager().SendRPC("eAI", "DayZPlayerInventory_OnEventForRemoteWeaponAICallback", new Param3<int, DayZPlayer, Magazine>(e.GetPackedType(), e.m_player, e.m_magazine));
			
			// Now that the RPC is sent to the clients, we need to compute the ballistics data and see about a hit.
			// We miiight want to do this in another thread???
			if (CanFire() && e.GetEventID() == WeaponEventID.TRIGGER) {
				
				Print("Round fired by " + e.m_player);
				
				// For now, we're just going to have a client handle this.
				// It will duplicate events if more than 1 person is on the server (not good)
				//array< PlayerIdentity > identities;
				//GetGame().GetPlayerIndentities(identities);
				PlayerBase p = PlayerBase.Cast(e.m_player);
				GetRPCManager().SendRPC("eAI", "DebugWeaponLocation", new Param1<Weapon_Base>(this), false, p.parent.m_FollowOrders.GetIdentity());
				
				// Eventually we will need to do this server side.
				
				// At this poin we are ready to register an event for the ballistics. BEcause of engine limitations we cannot directly 
				// poll the world space of the barrel to my knowledge. It is necessary to create two particles, attach them to the barrel,
				// _then wait a frame._ The new location of the Particles is then where the barrel is.
				
				// This was my second attempt server side...
				
				/*vector usti_hlavne_position = GetSelectionPositionLS("usti hlavne"); // front?
				vector konec_hlavne_position = GetSelectionPositionLS("konec hlavne"); // back?
				p_front = GetGame().CreateObject("SceneGraphPoint", usti_hlavne_position, true);
				p_back = GetGame().CreateObject("SceneGraphPoint", konec_hlavne_position, true);
				AddChild(p_front, -1);
				AddChild(p_back, -1);*/
				
				// This is the first way I tried to do this...
				
				//p_front = GetGame().CreateObject("Apple", usti_hlavne_position, true);
				/*p_front = Particle.Cast( GetGame().CreateObject("Particle", usti_hlavne_position, true) );
				p_front.SetSource(ParticleList.DEBUG_DOT);
				p_front.SetOrientation(vector.Zero);
				p_front.m_ForceOrientationRelativeToWorld = false;
				p_front.PlayParticle();
				AddChild(p_front, -1);
				p_front.Update();
				
				vector konec_hlavne_position = GetSelectionPositionLS("konec hlavne"); // back?
				p_back = GetGame().CreateObject("Apple", konec_hlavne_position, true);
				p_back = Particle.Cast( GetGame().CreateObject("Particle", konec_hlavne_position, true) );
				p_back.SetSource(ParticleList.DEBUG_DOT);
				p_back.m_ForceOrientationRelativeToWorld = false;
				p_back.PlayParticle();
				AddChild(p_back, -1);
				p_back.Update();*/
				
				Print("Waiting for postframe...");
				
				//SendBullet = true;
				toIgnore = e.m_player;
				// This is absolutely not the right way to do this... the correct way would be to wait for the next sFrame, but
				// that wasn't seeming to work
				//BallisticsPostFrame();
				//GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(BallisticsPostFrame, 5, false);
			}
			
			if (m_fsm.ProcessEvent(e) == ProcessEventResult.FSM_OK)
				return true;
				
			return false;
		
		// The rest of this is going to be client code. Also, for AI, clients should not sync events they receive to remote.
		} else if (!PlayerBase.Cast(e.m_player).isAI())
			SyncEventToRemote(e);
		
		// @NOTE: synchronous events not handled by fsm
		if (e.GetEventID() == WeaponEventID.SET_NEXT_MUZZLE_MODE)
		{
			SetNextMuzzleMode(GetCurrentMuzzle());
			return true;
		}

		if (m_fsm.ProcessEvent(e) == ProcessEventResult.FSM_OK)
			return true;

		//wpnDebugPrint("FSM refused to process event (no transition): src=" + GetCurrentState().ToString() + " event=" + e.ToString());
		return false;
	}
	/**@fn	ProcessWeaponAbortEvent
	 * @NOTE: warning: ProcessWeaponEvent can be called only within DayZPlayer::HandleWeapons (or ::CommandHandler)
	 **/
	override bool ProcessWeaponAbortEvent (WeaponEventBase e)
	{
		if (GetGame().IsServer() && PlayerBase.Cast(e.m_player).isAI()) { // This is very hacky... we need to check if the unit is also AI
			// Write the ctx that would normally be sent to the server... note we need to skip writing INPUT_UDT_WEAPON_REMOTE_EVENT
			// since this would normally be Read() and stripped away by the server before calling OnEventForRemoteWeapon
			ScriptRemoteInputUserData ctx = new ScriptRemoteInputUserData;
			GetRPCManager().SendRPC("eAI", "ToggleWeaponRaise", new Param3<int, DayZPlayer, Magazine>(e.GetPackedType(), e.m_player, e.m_magazine)); // This might need a separate RPC?
			if (m_fsm.ProcessEvent(e) == ProcessEventResult.FSM_OK)
				return true;
			return false;
		} else if (!PlayerBase.Cast(e.m_player).isAI())
			SyncEventToRemote(e);
		
		ProcessEventResult aa;
		m_fsm.ProcessAbortEvent(e, aa);
		return aa == ProcessEventResult.FSM_OK;
	}
}