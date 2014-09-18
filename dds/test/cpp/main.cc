// Simple C++ DDS test program.  Just creates a DomainParticipant.

#include <iostream>
#include "dds/dds_dcps.h"

int main()
{
	DDS_DomainParticipant	part;
	DDS_ReturnCode_t	error;

	std::cout << "Create a domain participant!" << std::endl;

	// Create a domain participant.
        part = DDS_DomainParticipantFactory_create_participant (
                                                0, NULL, NULL, 
						(DDS_StatusMask) 0);
        if (!part) {
		std::cout << "Particpant create failed!" << std::endl;
		return (1);
	}
	sleep (1);

	error = DDS_DomainParticipantFactory_delete_participant (part);
	if (error) {
		std::cout << "Participant delete returned error: " << error << std::endl;
		return (1);
	}
	return (0);
}
       	
