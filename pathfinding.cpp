
#include "stdafx.h"
#include "pathfinding.h"
#include "location.h"
#include "combat.h"

Pathfinding pathfindingSys;


class PathfindingReplacements : TempleFix
{
	macTempleFix(Bathfinding Functions)
	{
		 macReplaceFun(10040520, _ShouldUsePathnodesUsercallWrapper)
	}
} pathFindingReplacements;

float Pathfinding::pathLength(Path* path)
{
	float distTot;
	if (path->flags & PQF_UNK2)	return loc->distBtwnLocAndOffs(path->to, path->from) / 12.0f;
	distTot = 0;
	auto nodeFrom = path->from;
	for (int i = 0; i < path->nodeCount; i++)
	{
		auto nodeTo = path->nodes[i];
		distTot += loc->distBtwnLocAndOffs(nodeFrom, nodeTo);
		nodeFrom = nodeTo;
	}
	return distTot / 12.0f;
}

bool Pathfinding::pathQueryResultIsValid(PathQueryResult* pqr)
{
	return pqr != nullptr && pqr >= pathQArray && pqr < &pathQArray[pfCacheSize];
}

Pathfinding::Pathfinding() {
	static_assert(sizeof(PathQuery) == 0x50, "Path Query has the wrong size");
	static_assert(sizeof(Path) == 0x1a18, "Path has the wrong size");
	static_assert(sizeof(PathQueryResult) == 0x1a20, "Path Query Result has the wrong size");

	loc = &locSys;

	
	
	//rebase(PathStraightLineIsClear, 0x10040A90); // signed int __usercall PathStraightLineIsClear@<eax>(PathQueryResult *pqr@<ebx>, PathQuery *pathQ, LocAndOffsets from, LocAndOffsets to)
	rebase(PathDestIsClear, 0x10040C30);
	// rebase(FindPathStraightLine, 0x100427F0); //  signed int __usercall FindPathStraightLine@<eax>(PathQueryResult *pqr@<eax>, PathQuery *pq@<edi>)
	rebase(FindPath, 0x10043070);

	rebase(canPathToParty,0x10057F80); 
	rebase(pathQArray, 0x1186AC60);
	rebase(pathSthgFlag_10B3D5C8,0x10B3D5C8); 
}

uint32_t Pathfinding::ShouldUsePathnodes(PathQueryResult* pathQueryResult, PathQuery* pathQuery)
{
	bool result = false;
	LocAndOffsets from = pathQuery->from;
	if (!(pathQuery->flags & PQF_UNKNOWN4000h))
	{
		if (combatSys.isCombatActive())
		{
			if (locSys.distBtwnLocAndOffs(pathQueryResult->from, pathQueryResult->to) > (float)1200.0)
			{
				return true;
			}
			
		}
		else if (locSys.distBtwnLocAndOffs(pathQueryResult->from, pathQueryResult->to) > (float)800.0)
		{
			return true;
		}
	}
	return result;
}


uint32_t __cdecl _ShouldUsePathnodesCDecl(PathQueryResult * pqr, PathQuery * pq)
{
	return pathfindingSys.ShouldUsePathnodes(pqr, pq);
}


uint32_t __declspec(naked) _ShouldUsePathnodesUsercallWrapper()
{
	macAsmProl
		__asm{
		push ecx;
		push eax;
		call _ShouldUsePathnodesCDecl;
		add esp, 8;
	}
	macAsmEpil
	__asm  retn; 
}