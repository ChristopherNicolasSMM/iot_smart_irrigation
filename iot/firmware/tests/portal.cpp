#include "LocalPortalPolicy.h"
#include <cassert>
#include <cstring>
using namespace irrigation;
int main(){
 assert(!draftRejection(2,true,false,false,false,7,7));
 assert(!strcmp(draftRejection(1,true,false,false,false,7,7),"administrator_required"));
 assert(!strcmp(draftRejection(2,false,false,false,false,7,7),"csrf_invalid"));
 assert(!strcmp(draftRejection(2,true,true,false,false,7,7),"portal_is_configuration_authority"));
 assert(!strcmp(draftRejection(2,true,false,true,false,7,7),"portal_is_configuration_authority"));
 assert(!strcmp(draftRejection(2,true,false,false,true,7,7),"controller_not_idle"));
 assert(!strcmp(draftRejection(2,true,false,false,false,6,7),"revision_conflict"));
 assert(!strcmp(draftRejection(2,true,false,false,false,UINT32_MAX,UINT32_MAX),"revision_exhausted"));
 assert(!draftRejection(2,true,false,false,false,0,0));
}
