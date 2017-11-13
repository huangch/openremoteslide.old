/*
 *  OpenSlide, a library for reading whole slide image files
 *
 *  Copyright (c) 2007-2014 Carnegie Mellon University
 *  All rights reserved.
 *
 *  OpenSlide is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation, version 2.1.
 *
 *  OpenSlide is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with OpenSlide. If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#ifndef OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_XML_H_
#define OPENREMOTESLIDE_OPENREMOTESLIDE_DECODE_XML_H_

#include "openremoteslide-private.h"

#include <stdint.h>
#include <glib.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

/* libxml support code */

xmlDoc *_openremoteslide_xml_parse(const char *xml, GError **err);

bool _openremoteslide_xml_has_default_namespace(xmlDoc *doc, const char *ns);

int64_t _openremoteslide_xml_parse_int_attr(xmlNode *node, const char *name,
                                      GError **err);

double _openremoteslide_xml_parse_double_attr(xmlNode *node, const char *name,
                                        GError **err);

xmlXPathContext *_openremoteslide_xml_xpath_create(xmlDoc *doc);

xmlXPathObject *_openremoteslide_xml_xpath_eval(xmlXPathContext *ctx,
                                          const char *xpath);

xmlNode *_openremoteslide_xml_xpath_get_node(xmlXPathContext *ctx,
                                       const char *xpath);

char *_openremoteslide_xml_xpath_get_string(xmlXPathContext *ctx,
                                      const char *xpath);

void _openremoteslide_xml_set_prop_from_xpath(openremoteslide_t *osr,
                                        xmlXPathContext *ctx,
                                        const char *property_name,
                                        const char *xpath);

#endif
