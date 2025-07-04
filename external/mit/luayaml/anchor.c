/*
 * Copyright (C) 2025 Micro Systems Marc Balmer, CH-5073 Gipf-Oberfrick.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Process YAML anchors */

#include <lauxlib.h>
#include <string.h>
#include <yaml.h>

#include "anchor.h"

anchor_t *
anchor_init(void)
{
	anchor_t *root;

	root = malloc(sizeof(anchor_t));
	if (root) {
		root->anchor = NULL;
		root->next = NULL;
		root->ref = LUA_NOREF;
	}
	return root;
}

int
anchor_set(lua_State *L, anchor_t *root, yaml_char_t *anchor, int ref)
{
	anchor_t *r;

	/* Check if this is the first anchor */
	if (root->anchor == NULL) {
		root->anchor = (yaml_char_t *)strdup((char *)anchor);
		root->ref = ref;
		return 0;
	}

	for (; root; r = root, root = root->next) {
		if (!strcmp((const char *)root->anchor, (const char *)anchor)) {
			luaL_unref(L, LUA_REGISTRYINDEX, root->ref);
			root->ref = ref;
			return 0;
		}
	}

	/* Add anchor add the end of the list */
	r->next = anchor_init();
	r = r->next;
	if (r) {
		r->anchor = (yaml_char_t *)strdup((char *)anchor);
		r->ref = ref;
		return 0;
	} else
		return -1;
}

int
anchor_get(anchor_t *root, yaml_char_t *anchor)
{
	for (; root; root = root->next)
		if (root->anchor &&
		    !strcmp((const char *)root->anchor, (const char *)anchor))
			return root->ref;
	return LUA_NOREF;
}

void
anchor_free(lua_State *L, anchor_t *root)
{
	anchor_t *cur, *next;

	cur = root;

	while (cur) {
		next = cur->next;
		free(cur->anchor);
		luaL_unref(L, LUA_REGISTRYINDEX, cur->ref);
		free(cur);
		cur = next;
	}
}

#ifdef DEBUG
void
anchor_dump(anchor_t *root)
{
	printf("anchors:\n");
	for (; root; root = root->next)
		printf("%s\t%d\n", root->anchor, root->ref);
}
#endif
