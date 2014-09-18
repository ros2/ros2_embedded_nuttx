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

#ifndef __dcps_topic_h_
#define __dcps_topic_h_

unsigned char *dcps_key_data_get (Topic_t          *tp,
			          const void       *data,
				  int              dynamic,
				  int              secure,
				  unsigned char    buf [16],
				  size_t           *size,
				  DDS_ReturnCode_t *ret);

#endif /* !__dcps_topic_h_ */
