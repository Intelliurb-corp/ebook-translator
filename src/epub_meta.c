#include "epub.h"
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <limits.h>

epub_metadata_t* parse_epub_metadata(const char *root_dir) {
    char container_path[PATH_MAX];
    snprintf(container_path, sizeof(container_path), "%s/META-INF/container.xml", root_dir);

    xmlDocPtr doc = xmlReadFile(container_path, NULL, 0);
    if (!doc) {
        fprintf(stderr, "Error parsing container.xml\n");
        return NULL;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(xpathCtx, (const xmlChar*)"ns", (const xmlChar*)"urn:oasis:names:tc:opendocument:xmlns:container");
    
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar*)"//ns:rootfile/@full-path", xpathCtx);
    if (xmlXPathNodeSetIsEmpty(xpathObj->nodesetval)) {
        fprintf(stderr, "No rootfile found in container.xml\n");
        xmlXPathFreeObject(xpathObj);
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
        return NULL;
    }

    char *opf_rel_path = (char*)xmlNodeListGetString(doc, xpathObj->nodesetval->nodeTab[0]->xmlChildrenNode, 1);
    char opf_path[PATH_MAX];
    snprintf(opf_path, sizeof(opf_path), "%s/%s", root_dir, opf_rel_path);

    xmlXPathFreeObject(xpathObj);
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    // Parsing OPF
    xmlDocPtr opf_doc = xmlReadFile(opf_path, NULL, 0);
    if (!opf_doc) {
        fprintf(stderr, "Error parsing OPF file: %s\n", opf_path);
        xmlFree(opf_rel_path);
        return NULL;
    }

    epub_metadata_t *meta = calloc(1, sizeof(epub_metadata_t));
    xmlXPathContextPtr opfXpathCtx = xmlXPathNewContext(opf_doc);
    xmlXPathRegisterNs(opfXpathCtx, (const xmlChar*)"opf", (const xmlChar*)"http://www.idpf.org/2007/opf");
    xmlXPathRegisterNs(opfXpathCtx, (const xmlChar*)"dc", (const xmlChar*)"http://purl.org/dc/elements/1.1/");

    // Title
    xmlXPathObjectPtr titleObj = xmlXPathEvalExpression((const xmlChar*)"//dc:title/text()", opfXpathCtx);
    if (!xmlXPathNodeSetIsEmpty(titleObj->nodesetval))
        meta->title = strdup((char*)titleObj->nodesetval->nodeTab[0]->content);
    xmlXPathFreeObject(titleObj);

    // Set base path from OPF path
    // path is like "OEBPS/content.opf" or "content.opf"
    char *last_slash = strrchr(opf_rel_path, '/');
    if (last_slash) {
        size_t len = last_slash - opf_rel_path;
        meta->base_dir = malloc(len + 1);
        strncpy(meta->base_dir, opf_rel_path, len);
        meta->base_dir[len] = '\0';
    } else {
        meta->base_dir = strdup("");
    }

    // Manifest
    xmlXPathObjectPtr manifestObj = xmlXPathEvalExpression((const xmlChar*)"//opf:manifest/opf:item", opfXpathCtx);
    if (!xmlXPathNodeSetIsEmpty(manifestObj->nodesetval)) {
        meta->manifest_count = manifestObj->nodesetval->nodeNr;
        meta->manifest = calloc(meta->manifest_count, sizeof(epub_item_t));
        for (int i = 0; i < meta->manifest_count; i++) {
            xmlNodePtr node = manifestObj->nodesetval->nodeTab[i];
            meta->manifest[i].name = (char*)xmlGetProp(node, (const xmlChar*)"id");
            meta->manifest[i].href = (char*)xmlGetProp(node, (const xmlChar*)"href");
            meta->manifest[i].media_type = (char*)xmlGetProp(node, (const xmlChar*)"media-type");
        }
    }
    xmlXPathFreeObject(manifestObj);

    // Spine
    xmlXPathObjectPtr spineObj = xmlXPathEvalExpression((const xmlChar*)"//opf:spine/opf:itemref", opfXpathCtx);
    if (!xmlXPathNodeSetIsEmpty(spineObj->nodesetval)) {
        meta->spine_count = spineObj->nodesetval->nodeNr;
        meta->spine = calloc(meta->spine_count, sizeof(char*));
        for (int i = 0; i < meta->spine_count; i++) {
            meta->spine[i] = (char*)xmlGetProp(spineObj->nodesetval->nodeTab[i], (const xmlChar*)"idref");
        }
    }
    xmlXPathFreeObject(spineObj);

    printf("Parsed EPUB: %s (Items: %d)\n", meta->title ? meta->title : "Unknown", meta->manifest_count);

    xmlXPathFreeContext(opfXpathCtx);
    xmlFree(opf_rel_path);
    xmlFreeDoc(opf_doc);
    return meta;
}

void free_epub_metadata(epub_metadata_t *meta) {
    if (!meta) return;
    free(meta->title);
    free(meta->author);
    free(meta->base_dir);
    for (int i = 0; i < meta->spine_count; i++) free(meta->spine[i]);
    free(meta->spine);
    for (int i = 0; i < meta->manifest_count; i++) {
        free(meta->manifest[i].name);
        free(meta->manifest[i].href);
        free(meta->manifest[i].media_type);
    }
    free(meta->manifest);
    free(meta);
}
