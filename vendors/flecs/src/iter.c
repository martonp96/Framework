#include "private_api.h"

#define INIT_CACHE(it, f, term_count)\
    if (!it->f && term_count) {\
        if (term_count < ECS_TERM_CACHE_SIZE) {\
            it->f = it->cache.f;\
            it->cache.f##_alloc = false;\
        } else {\
            it->f = ecs_os_calloc(ECS_SIZEOF(*(it->f)) * term_count);\
            it->cache.f##_alloc = true;\
        }\
    }

#define FINI_CACHE(it, f)\
    if (it->f) {\
        if (it->cache.f##_alloc) {\
            ecs_os_free((void*)it->f);\
        }\
    }   

void flecs_iter_init(
    ecs_iter_t *it)
{
    INIT_CACHE(it, ids, it->term_count);
    INIT_CACHE(it, columns, it->term_count);
    INIT_CACHE(it, subjects, it->term_count);
    INIT_CACHE(it, sizes, it->term_count);
    INIT_CACHE(it, ptrs, it->term_count);
    INIT_CACHE(it, match_indices, it->term_count);

    it->is_valid = true;
}

void flecs_iter_fini(
    ecs_iter_t *it)
{
    ecs_assert(it->is_valid == true, ECS_INVALID_PARAMETER, NULL);
    it->is_valid = false;

    FINI_CACHE(it, ids);
    FINI_CACHE(it, columns);
    FINI_CACHE(it, subjects);
    FINI_CACHE(it, sizes);
    FINI_CACHE(it, ptrs);
    FINI_CACHE(it, match_indices);
}

/* --- Public API --- */

void* ecs_term_w_size(
    const ecs_iter_t *it,
    size_t size,
    int32_t term)
{
    ecs_assert(it->is_valid, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(!size || ecs_term_size(it, term) == size, 
        ECS_INVALID_PARAMETER, NULL);

    (void)size;

    if (!term) {
        return it->entities;
    }
    
    if (!it->ptrs) {
        return NULL;
    }

    return it->ptrs[term - 1];
}

bool ecs_term_is_readonly(
    const ecs_iter_t *it,
    int32_t term_index)
{
    ecs_assert(it->is_valid, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(term_index > 0, ECS_INVALID_PARAMETER, NULL);

    ecs_term_t *term = &it->terms[term_index - 1];
    ecs_assert(term != NULL, ECS_INVALID_PARAMETER, NULL);
    
    if (term->inout == EcsIn) {
        return true;
    } else {
        ecs_term_id_t *subj = &term->args[0];

        if (term->inout == EcsInOutDefault) {
            if (subj->entity != EcsThis) {
                return true;
            }

            if ((subj->set.mask != EcsSelf) && 
                (subj->set.mask != EcsDefaultSet)) 
            {
                return true;
            }
        }
    }

    return false;
}

int32_t ecs_iter_find_column(
    const ecs_iter_t *it,
    ecs_entity_t component)
{
    ecs_assert(it->is_valid, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(it->table != NULL, ECS_INVALID_PARAMETER, NULL);
    return ecs_type_index_of(it->table->type, 0, component);
}

void* ecs_iter_column_w_size(
    const ecs_iter_t *it,
    size_t size,
    int32_t column_index)
{
    ecs_assert(it->is_valid, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(it->table != NULL, ECS_INVALID_PARAMETER, NULL);
    (void)size;
    
    ecs_table_t *table = it->table;
    ecs_assert(column_index < ecs_vector_count(table->type), 
        ECS_INVALID_PARAMETER, NULL);
    
    if (table->column_count <= column_index) {
        return NULL;
    }

    ecs_column_t *columns = table->storage.columns;
    ecs_column_t *column = &columns[column_index];
    ecs_assert(!size || (ecs_size_t)size == column->size, ECS_INVALID_PARAMETER, NULL);

    return ecs_vector_first_t(column->data, column->size, column->alignment);
}

size_t ecs_iter_column_size(
    const ecs_iter_t *it,
    int32_t column_index)
{
    ecs_assert(it->is_valid, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(it->table != NULL, ECS_INVALID_PARAMETER, NULL);
    
    ecs_table_t *table = it->table;
    ecs_assert(column_index < ecs_vector_count(table->type), 
        ECS_INVALID_PARAMETER, NULL);

    if (table->column_count <= column_index) {
        return 0;
    }

    ecs_column_t *columns = table->storage.columns;
    ecs_column_t *column = &columns[column_index];
    
    return flecs_to_size_t(column->size);
}

char* ecs_iter_str(
    const ecs_iter_t *it)
{
    ecs_world_t *world = it->world;
    ecs_strbuf_t buf = ECS_STRBUF_INIT;
    int i;

    if (it->term_count) {
        ecs_strbuf_list_push(&buf, "term: ", ",");
        for (i = 0; i < it->term_count; i ++) {
            ecs_id_t id = ecs_term_id(it, i + 1);
            char *str = ecs_id_str(world, id);
            ecs_strbuf_list_appendstr(&buf, str);
            ecs_os_free(str);
        }
        ecs_strbuf_list_pop(&buf, "\n");

        ecs_strbuf_list_push(&buf, "subj: ", ",");
        for (i = 0; i < it->term_count; i ++) {
            ecs_entity_t subj = ecs_term_source(it, i + 1);
            char *str = ecs_get_fullpath(world, subj);
            ecs_strbuf_list_appendstr(&buf, str);
            ecs_os_free(str);
        }
        ecs_strbuf_list_pop(&buf, "\n");
    }

    if (it->variable_count) {
        int32_t actual_count = 0;
        for (i = 0; i < it->variable_count; i ++) {
            const char *var_name = it->variable_names[i];
            if (var_name[0] == '_') {
                /* Skip anonymous variables */
                continue;
            }
            
            if (var_name[0] == '.') {
                /* Skip this */
                continue;
            }

            ecs_entity_t var = it->variables[i];
            if (!var) {
                /* Skip table variables */
                continue;
            }

            if (!actual_count) {
                ecs_strbuf_list_push(&buf, "vars: ", ",");
            }

            char *str = ecs_get_fullpath(world, var);
            ecs_strbuf_list_append(&buf, "%s=%s", var_name, str);
            ecs_os_free(str);

            actual_count ++;
        }
        if (actual_count) {
            ecs_strbuf_list_pop(&buf, "\n");
        }
    }

    if (it->count) {
        ecs_strbuf_appendstr(&buf, "this:\n");
        for (i = 0; i < it->count; i ++) {
            ecs_entity_t e = it->entities[i];
            char *str = ecs_get_fullpath(world, e);
            ecs_strbuf_appendstr(&buf, "    - ");
            ecs_strbuf_appendstr(&buf, str);
            ecs_strbuf_appendstr(&buf, "\n");
            ecs_os_free(str);
        }
    }

    return ecs_strbuf_get(&buf);
}
