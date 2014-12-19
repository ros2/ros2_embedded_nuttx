/*
 * Copyright (c) 2014 - Qeo LLC
 *
 * The source code form of this Qeo Open Source Project component is subject
 * to the terms of the Clear BSD license.
 *
 * You can redistribute it and/or modify it under the terms of the Clear BSD
 * License (http://directory.fsf.org/wiki/License:ClearBSD). See LICENSE file
 * for more details.
 *
 * The Qeo Open Source Project also includes third party Open Source Software.
 * See LICENSE file for more details.
 */


/*#######################################################################
#                   STATIC FUNCTION IMPLEMENTATION                      #
########################################################################*/

/*#######################################################################
#                   PUBLIC FUNCTION IMPLEMENTATION                      #
########################################################################*/
char* get_success_messages(int id){
    return success_messages[id];
}

char* get_error_messages(int id){
    return error_messages[id];
}

char* get_services_messages(int id){
    return services_messages[id];
}
