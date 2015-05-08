/* annoStreamDb -- subclass of annoStreamer for database tables */

#ifndef ANNOSTREAMDBFACTORSOURCE_H
#define ANNOSTREAMDBFACTORSOURCE_H

#include "annoStreamer.h"

struct asObject *annoStreamDbFactorSourceAsObj();
/* Return an autoSql object that describs fields of a joining query on a factorSource table
 * and its inputs. */

struct annoStreamer *annoStreamDbFactorSourceNew(char *db, char *trackTable, char *sourceTable,
						 char *inputsTable, struct annoAssembly *aa,
						 int maxOutRows);
/* Create an annoStreamer (subclass) object using three database tables:
 * trackTable: the table for a track with type factorSource (bed5 + exp{Count,Nums,Scores})
 * sourceTable: trackTable's tdb setting sourceTable; expNums -> source name "cellType+lab+antibody"
 * inputsTable: trackTable's tdb setting inputTrackTable; source name -> cellType, treatment, etc.
 */

#endif//ndef ANNOSTREAMDBFACTORSOURCE_H
