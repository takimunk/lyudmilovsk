#pragma once

#include "CoreMinimal.h"

class IAtonShell;
class SAtonProgram;

/**
 * The programs of the lab network «ЛАБ-2» (docs/aton_lab.md): the session (guest or the duty shift, by its password),
 * the lab's old mail, the text adventure «Ночная вахта», chess against the machine, the weather station's live
 * readings and the telescope's queue. Their rules are plain C++ in AtonOS/Lab (tools/atonsim checks them outside the
 * engine); what the hero did is ULyudAtonLab's state, kept in the save.
 */
namespace AtonLabUI
{
	/** The program for Id, or null when Id is not one of the lab's. */
	TSharedPtr<SAtonProgram> MakeProgram(FName Id, IAtonShell& Shell);
}
