/**
 * @file flecs.h
 * @brief Flecs public API.
 *
 * This file contains the public API for Flecs.
 */

#ifndef FLECS_H
#define FLECS_H

/**
 * @defgroup options API toggles & constants
 * @{
 */

/* Customizable precision for floating point operations (such as time ops) */
#ifndef FLECS_FLOAT
#define FLECS_FLOAT float
#endif

/* FLECS_LEGACY should be defined when building for C89 */
// #define FLECS_LEGACY

/* FLECS_NO_DEPRECATED_WARNINGS disables deprecated warnings */
#define FLECS_NO_DEPRECATED_WARNINGS

/* FLECS_SANITIZE enables expensive checks that can detect issues early */
#ifndef NDEBUG
#define FLECS_SANITIZE
#endif

/* FLECS_CUSTOM_BUILD should be defined when manually selecting features */
// #define FLECS_CUSTOM_BUILD

/* If this is a regular, non-custom build, build all modules and addons. */
#ifndef FLECS_CUSTOM_BUILD
// #define FLECS_C          /* C API convenience macro's, always enabled */
#define FLECS_CPP           /* C++ API */
#define FLECS_MODULE        /* Module support */
#define FLECS_PARSER        /* String parser for queries */
#define FLECS_PLECS         /* ECS data definition format */
#define FLECS_RULES         /* Constraint solver for advanced queries */
#define FLECS_SNAPSHOT      /* Snapshot & restore ECS data */
#define FLECS_STATS         /* Keep track of runtime statistics */
#define FLECS_SYSTEM        /* System support */
#define FLECS_PIPELINE      /* Pipeline support */
#define FLECS_TIMER         /* Timer support */
#endif // ifndef FLECS_CUSTOM_BUILD

/** @} */

#include "flecs/private/api_defines.h"
#include "flecs/private/log.h"              /* Logging API */
#include "flecs/private/vector.h"           /* Vector datatype */
#include "flecs/private/map.h"              /* Map */
#include "flecs/private/strbuf.h"           /* String builder */
#include "flecs/os_api.h"  /* Abstraction for operating system functions */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup api_types Basic API types
 * @{
 */

/** An object with a mixin table. */
typedef void ecs_poly_t;

/** An id. Ids are the things that can be added to an entity. An id can be an
 * entity or pair, and can have an optional role. */
typedef uint64_t ecs_id_t;

/** An entity identifier. */
typedef ecs_id_t ecs_entity_t;

/** A vector containing component identifiers used to describe a type. */
typedef const ecs_vector_t* ecs_type_t;

/** A world is the container for all ECS data and supporting features. */
typedef struct ecs_world_t ecs_world_t;

/** A query allows for cached iteration over ECS data */
typedef struct ecs_query_t ecs_query_t;

/** A filter allows for uncached, ad hoc iteration over ECS data */
typedef struct ecs_filter_t ecs_filter_t;

/** A rule implements a non-trivial filter */
typedef struct ecs_rule_t ecs_rule_t;

/** A trigger reacts to events matching a single filter term */
typedef struct ecs_trigger_t ecs_trigger_t;

/** An observer reacts to events matching multiple filter terms */
typedef struct ecs_observer_t ecs_observer_t;

/** An observable contains lists of triggers for specific events/components */
typedef struct ecs_observable_t ecs_observable_t;

/* An iterator lets an application iterate entities across tables. */
typedef struct ecs_iter_t ecs_iter_t;

/** Refs cache data that lets them access components faster than ecs_get. */
typedef struct ecs_ref_t ecs_ref_t;

/** @} */


/**
 * @defgroup constants API constants
 * @{
 */

/* Maximum number of components to add/remove in a single operation */
#define ECS_MAX_ADD_REMOVE (32)

/* Maximum number of terms cached in static arrays */
#define ECS_TERM_CACHE_SIZE (8)

/* Maximum number of terms in desc (larger, as these are temp objects) */
#define ECS_TERM_DESC_CACHE_SIZE (16)

/* Maximum number of events to set in static array of trigger descriptor */
#define ECS_TRIGGER_DESC_EVENT_COUNT_MAX (8)

/** @} */


/**
 * @defgroup function_types Function Types
 * @{
 */

/** Action callback for systems and triggers */
typedef void (*ecs_iter_action_t)(
    ecs_iter_t *it);

typedef bool (*ecs_iter_next_action_t)(
    ecs_iter_t *it);  

/** Callback used for sorting components */
typedef int (*ecs_order_by_action_t)(
    ecs_entity_t e1,
    const void *ptr1,
    ecs_entity_t e2,
    const void *ptr2);

/** Callback used for ranking types */
typedef uint64_t (*ecs_group_by_action_t)(
    ecs_world_t *world,
    ecs_type_t type,
    ecs_id_t id,
    void *ctx);

/** Initialization action for modules */
typedef void (*ecs_module_action_t)(
    ecs_world_t *world);    

/** Action callback on world exit */
typedef void (*ecs_fini_action_t)(
    ecs_world_t *world,
    void *ctx);

/** Function to cleanup context data */
typedef void (*ecs_ctx_free_t)(
    void *ctx);

/** Callback used for sorting values */
typedef int (*ecs_compare_action_t)(
    const void *ptr1,
    const void *ptr2);

/** Callback used for hashing values */
typedef uint64_t (*ecs_hash_value_action_t)(
    const void *ptr); 

/** @} */


/**
 * @defgroup filter_types Types used to describe filters, terms and triggers
 * @{
 */

/** Set flags describe if & how a matched entity should be substituted */
#define EcsDefaultSet   (0)  /* Default set, SuperSet|Self for This subject */
#define EcsSelf         (1)  /* Select self (inclusive) */
#define EcsSuperSet     (2)  /* Select superset until predicate match */
#define EcsSubSet       (4)  /* Select subset until predicate match */
#define EcsCascade      (8)  /* Use breadth-first ordering of relations */
#define EcsAll          (16) /* Walk full super/subset, regardless of match */
#define EcsParent       (32) /* Shortcut for SuperSet(ChildOf) */
#define EcsNothing      (64) /* Select from nothing */

/** Specify read/write access for term */
typedef enum ecs_inout_kind_t {
    EcsInOutDefault,
    EcsInOut,
    EcsIn,
    EcsOut
} ecs_inout_kind_t;

/** Specifies whether term identifier is a variable or entity */
typedef enum ecs_var_kind_t {
    EcsVarDefault,      /* Variable if name is all caps, otherwise an entity */
    EcsVarIsEntity,     /* Term is an entity */
    EcsVarIsVariable    /* Term is a variable */
} ecs_var_kind_t;

/** Type describing an operator used in an signature of a system signature */
typedef enum ecs_oper_kind_t {
    EcsAnd,         /* The term must match */
    EcsOr,          /* One of the terms in an or chain must match */
    EcsNot,         /* The term must not match */
    EcsOptional,    /* The term may match */
    EcsAndFrom,     /* Term must match all components from term id */
    EcsOrFrom,      /* Term must match at least one component from term id */
    EcsNotFrom      /* Term must match none of the components from term id */
} ecs_oper_kind_t;

/** Substitution with set parameters.
 * These parameters allow for substituting a term id with its super- or subsets
 * for a specified relationship. This enables functionality such as selecting
 * components from a base (IsA) or a parent (ChildOf) in a single term */
typedef struct ecs_term_set_t {
    ecs_entity_t relation;      /* Relationship to substitute (default = IsA) */
    uint8_t mask;               /* Substitute as self, subset, superset */
    int32_t min_depth;          /* Min depth of subset/superset substitution */
    int32_t max_depth;          /* Max depth of subset/superset substitution */
} ecs_term_set_t;

/** Type that describes a single identifier in a term */
typedef struct ecs_term_id_t {
    ecs_entity_t entity;        /* Entity (default = This) */
    char *name;                 /* Name (default = ".") */
    ecs_var_kind_t var;         /* Is id a variable (default yes if name is 
                                 * all caps & entity is 0) */
    ecs_term_set_t set;         /* Set substitution parameters */
} ecs_term_id_t;

/** Type that describes a single column in the system signature */
typedef struct ecs_term_t {
    ecs_id_t id;                /* Can be used instead of pred, args and role to
                                 * set component/pair id. If not set, it will be 
                                 * computed from predicate, object. If set, the
                                 * subject cannot be set, or be set to This. */
    
    ecs_inout_kind_t inout;     /* Access to contents matched with term */
    ecs_term_id_t pred;         /* Predicate of term */
    ecs_term_id_t args[2];      /* Subject (0), object (1) of term */
    ecs_oper_kind_t oper;       /* Operator of term */
    ecs_id_t role;              /* Role of term */
    char *name;                 /* Name of term */

    int32_t index;              /* Computed term index in filter which takes 
                                 * into account folded OR terms */

    bool move;                  /* When true, this signals to ecs_term_copy that
                                 * the resources held by this term may be moved
                                 * into the destination term. */
} ecs_term_t;

/** Filters alllow for ad-hoc quick filtering of entity tables. */
struct ecs_filter_t {
    ecs_term_t *terms;         /* Array containing terms for filter */
    int32_t term_count;        /* Number of elements in terms array */
    int32_t term_count_actual; /* Processed count, which folds OR terms */

    ecs_term_t term_cache[ECS_TERM_CACHE_SIZE]; /* Cache for small filters */
    bool term_cache_used;

    bool match_this;           /* Has terms that match EcsThis */
    bool match_only_this;      /* Has only terms that match EcsThis */
    bool match_prefab;         /* Does filter match prefabs */
    bool match_disabled;       /* Does filter match disabled entities */
    
    char *name;                /* Name of filter (optional) */
    char *expr;                /* Expression of filter (if provided) */
};

/** A trigger reacts to events matching a single term */
struct ecs_trigger_t {
    ecs_term_t term;            /* Term describing the trigger condition id */

    /* Trigger events */
    ecs_entity_t events[ECS_TRIGGER_DESC_EVENT_COUNT_MAX];
    int32_t event_count;

    ecs_iter_action_t action;   /* Callback */

    void *ctx;                  /* Callback context */
    void *binding_ctx;          /* Binding context (for language bindings) */

    ecs_ctx_free_t ctx_free;    /* Callback to free ctx */
    ecs_ctx_free_t binding_ctx_free; /* Callback to free binding_ctx */
    
    ecs_entity_t entity;        /* Trigger entity */
    ecs_entity_t self;          /* Entity associated with observer */

    ecs_observable_t *observable;  /* Observable for trigger */

    bool match_prefab;          /* Should trigger ignore prefabs */
    bool match_disabled;        /* Should trigger ignore disabled entities */

    uint64_t id;                /* Internal id */
};

/* An observer reacts to events matching a filter */
struct ecs_observer_t {
    ecs_filter_t filter;

    /* Triggers created by observer (array size same as number of terms) */
    ecs_entity_t *triggers;

    /* Observer events */
    ecs_entity_t events[ECS_TRIGGER_DESC_EVENT_COUNT_MAX];
    int32_t event_count;   
    
    ecs_iter_action_t action;   /* Callback */

    void *ctx;                  /* Callback context */
    void *binding_ctx;          /* Binding context (for language bindings) */

    ecs_ctx_free_t ctx_free;    /* Callback to free ctx */
    ecs_ctx_free_t binding_ctx_free; /* Callback to free binding_ctx */
    
    ecs_entity_t entity;        /* Observer entity */
    ecs_entity_t self;          /* Entity associated with observer */

    ecs_observable_t *observable;  /* Observable for observer */

    uint64_t id;                /* Internal id */  
    int32_t last_event_id;      /* Last handled event id */  

    bool is_monitor;            /* If true, the observer only triggers when the
                                 * filter did not match with the entity before
                                 * the event happened. */
};

/** @} */


#include "flecs/private/api_types.h"        /* Supporting API types */
#include "flecs/private/api_support.h"      /* Supporting API functions */
#include "flecs/type.h"                     /* Type API */


/**
 * @defgroup desc_types Types used for creating API constructs
 * @{
 */

/** Used with ecs_entity_init */
typedef struct ecs_entity_desc_t { 
    ecs_entity_t entity; /* Optional existing entity handle. */

    const char *name;    /* Name of the entity. If no entity is provided, an 
                          * entity with this name will be looked up first. When
                          * an entity is provided, the name will be verified
                          * with the existing entity. */

    const char *sep;     /* Optional custom separator for hierarchical names */
    const char *root_sep; /* Optional, used for identifiers relative to root */

    const char *symbol;  /* Optional entity symbol. A symbol is an unscoped
                          * identifier that can be used to lookup an entity. The
                          * primary use case for this is to associate the entity
                          * with a language identifier, such as a type or 
                          * function name, where these identifiers differ from
                          * the name they are registered with in flecs. For 
                          * example, C type "EcsPosition" might be registered
                          * as "flecs.components.transform.Position", with the
                          * symbol set to "EcsPosition". */

    bool use_low_id;     /* When set to true, a low id (typically reserved for
                          * components) will be used to create the entity, if
                          * no id is specified. */

    /* Array of ids to add to the new or existing entity. */
    ecs_id_t add[ECS_MAX_ADD_REMOVE];

    /* String expression with components to add */
    const char *add_expr;
} ecs_entity_desc_t;


/** Used with ecs_component_init. */
typedef struct ecs_component_desc_t {
    ecs_entity_desc_t entity;           /* Parameters for component entity */
    size_t size;                        /* Component size */
    size_t alignment;                   /* Component alignment */
} ecs_component_desc_t;


/** Used with ecs_type_init. */
typedef struct ecs_type_desc_t {
    ecs_entity_desc_t entity;           /* Parameters for type entity */
    ecs_id_t ids[ECS_MAX_ADD_REMOVE];   /* Ids to include in type */
    const char *ids_expr;               /* Id expression to include in type */
} ecs_type_desc_t;


/** Used with ecs_filter_init. */
typedef struct ecs_filter_desc_t {
    /* Terms of the filter. If a filter has more terms than 
     * ECS_TERM_CACHE_SIZE use terms_buffer */
    ecs_term_t terms[ECS_TERM_DESC_CACHE_SIZE];

    /* For filters with lots of terms an outside array can be provided. */
    ecs_term_t *terms_buffer;
    int32_t terms_buffer_count;

    /* Use default substitution rules where appropriate. When enabled, the
     * filter will determine based on whether the predicate is transitive and/or
     * final whether IsA substitution is required. */
    bool substitute_default;

    /* Filter expression. Should not be set at the same time as terms array */
    const char *expr;

    /* Optional name of filter, used for debugging. If a filter is created for
     * a system, the provided name should match the system name. */
    const char *name;
} ecs_filter_desc_t;


/** Used with ecs_query_init. */
typedef struct ecs_query_desc_t {
    /* Filter for the query */
    ecs_filter_desc_t filter;

    /* Component to be used by order_by */
    ecs_entity_t order_by_component;

    /* Callback used for ordering query results. If order_by_id is 0, the 
     * pointer provided to the callback will be NULL. If the callback is not
     * set, results will not be ordered. */
    ecs_order_by_action_t order_by;

    /* Id to be used by group_by. This id is passed to the group_by function and
     * can be used identify the part of an entity type that should be used for
     * grouping. */
    ecs_id_t group_by_id;

    /* Callback used for grouping results. If the callback is not set, results
     * will not be grouped. When set, this callback will be used to calculate a
     * "rank" for each entity (table) based on its components. This rank is then
     * used to sort entities (tables), so that entities (tables) of the same
     * rank are "grouped" together when iterated. */
    ecs_group_by_action_t group_by;

    /* Context to pass to group_by */
    void *group_by_ctx;

    /* Function to free group_by_ctx */
    ecs_ctx_free_t group_by_ctx_free;

    /* If set, the query will be created as a subquery. A subquery matches at
     * most a subset of its parent query. Subqueries do not directly receive
     * (table) notifications from the world. Instead parent queries forward
     * results to subqueries. This can improve matching performance, as fewer
     * queries need to be matched with new tables.
     * Subqueries can be nested. */
    ecs_query_t *parent;

    /* INTERNAL PROPERTY - system to be associated with query. Do not set, as 
     * this will change in future versions. */
    ecs_entity_t system;
} ecs_query_desc_t;


/** Used with ecs_trigger_init. */
typedef struct ecs_trigger_desc_t {
    /* Entity to associate with trigger */
    ecs_entity_desc_t entity;

    /* Term specifying the id to subscribe for */
    ecs_term_t term;

    /* Filter expression. May only contain a single term. If this field is set,
     * the term field is ignored. */
    const char *expr;

    /* Events to trigger on (OnAdd, OnRemove, OnSet, UnSet) */
    ecs_entity_t events[ECS_TRIGGER_DESC_EVENT_COUNT_MAX];

    /* Should trigger ignore prefabs & disabled entities */
    bool match_prefab;
    bool match_disabled;

    /* Callback to invoke on an event */
    ecs_iter_action_t callback;

    /* Associate with entity */
    ecs_entity_t self;

    /* User context to pass to callback */
    void *ctx;

    /* Context to be used for language bindings */
    void *binding_ctx;
    
    /* Callback to free ctx */
    ecs_ctx_free_t ctx_free;

    /* Callback to free binding_ctx */     
    ecs_ctx_free_t binding_ctx_free;

    /* Observable with which to register the trigger */
    ecs_poly_t *observable;
} ecs_trigger_desc_t;


/** Used with ecs_observer_init. */
typedef struct ecs_observer_desc_t {
    /* Entity to associate with observer */
    ecs_entity_desc_t entity;

    /* Filter for observer */
    ecs_filter_desc_t filter;

    /* Events to observe (OnAdd, OnRemove, OnSet, UnSet) */
    ecs_entity_t events[ECS_TRIGGER_DESC_EVENT_COUNT_MAX];

    /* Callback to invoke on an event */
    ecs_iter_action_t callback;

    /* Associate with entity */
    ecs_entity_t self;

    /* User context to pass to callback */
    void *ctx;

    /* Context to be used for language bindings */
    void *binding_ctx;
    
    /* Callback to free ctx */
    ecs_ctx_free_t ctx_free;

    /* Callback to free binding_ctx */     
    ecs_ctx_free_t binding_ctx_free;

    /* Observable with which to register the trigger */
    ecs_poly_t *observable;  
} ecs_observer_desc_t;

/** @} */


/**
 * @defgroup builtin_components Builtin components
 * @{
 */

/** A (string) identifier. Used as pair with EcsName and EcsSymbol tags */
typedef struct EcsIdentifier {
    char *value;
    ecs_size_t length;
    uint64_t hash;    
} EcsIdentifier;

/** Component information. */
typedef struct EcsComponent {
    ecs_size_t size;           /* Component size */
    ecs_size_t alignment;      /* Component alignment */
} EcsComponent;

/** Component that stores an ecs_type_t. 
 * This component allows for the creation of entities that represent a type, and
 * therefore the creation of named types. */
typedef struct EcsType {
    ecs_type_t type;        /* Preserved nested types */
    ecs_type_t normalized;  /* Union of type and nested AND types */
} EcsType;

/** Component that contains lifecycle callbacks for a component. */
struct EcsComponentLifecycle {
    ecs_xtor_t ctor;            /* ctor */
    ecs_xtor_t dtor;            /* dtor */
    ecs_copy_t copy;            /* copy assignment */
    ecs_move_t move;            /* move assignment */

    void *ctx;                  /* User defined context */

    /* Ctor + copy */
    ecs_copy_ctor_t copy_ctor;

    /* Ctor + move */  
    ecs_move_ctor_t move_ctor;

    /* Ctor + move + dtor (or move_ctor + dtor).
     * This combination is typically used when a component is moved from one
     * location to a new location, like when it is moved to a new table. If
     * not set explicitly it will be derived from other callbacks. */
    ecs_move_ctor_t ctor_move_dtor;

    /* Move + dtor.
     * This combination is typically used when a component is moved from one
     * location to an existing location, like what happens during a remove. If
     * not set explicitly it will be derived from other callbacks. */
    ecs_move_ctor_t move_dtor;

    /* Callback that is invoked when an instance of the component is set. This
     * callback is invoked before triggers are invoked, and enable the component
     * to respond to changes on itself before others can. */
    ecs_on_set_t on_set;
};

/** Component that stores reference to trigger */
typedef struct EcsTrigger {
    const ecs_trigger_t *trigger;
} EcsTrigger;

/** Component that stores reference to observer */
typedef struct EcsObserver {
    const ecs_observer_t *observer;
} EcsObserver;

/** Component for storing a query */
typedef struct EcsQuery {
    ecs_query_t *query;
} EcsQuery;

/** @} */


/**
 * @defgroup misc_types Miscalleneous types
 * @{
 */

/** Type that contains information about the world. */
typedef struct ecs_world_info_t {
    ecs_entity_t last_component_id;   /* Last issued component entity id */
    ecs_entity_t last_id;             /* Last issued entity id */
    ecs_entity_t min_id;              /* First allowed entity id */
    ecs_entity_t max_id;              /* Last allowed entity id */

    FLECS_FLOAT delta_time_raw;       /* Raw delta time (no time scaling) */
    FLECS_FLOAT delta_time;           /* Time passed to or computed by ecs_progress */
    FLECS_FLOAT time_scale;           /* Time scale applied to delta_time */
    FLECS_FLOAT target_fps;           /* Target fps */
    FLECS_FLOAT frame_time_total;     /* Total time spent processing a frame */
    FLECS_FLOAT system_time_total;    /* Total time spent in systems */
    FLECS_FLOAT merge_time_total;     /* Total time spent in merges */
    FLECS_FLOAT world_time_total;     /* Time elapsed in simulation */
    FLECS_FLOAT world_time_total_raw; /* Time elapsed in simulation (no scaling) */
    
    int32_t frame_count_total;        /* Total number of frames */
    int32_t merge_count_total;        /* Total number of merges */
    int32_t pipeline_build_count_total; /* Total number of pipeline builds */
    int32_t systems_ran_frame;  /* Total number of systems ran in last frame */
} ecs_world_info_t;

/** @} */

/* Only include deprecated definitions if deprecated addon is required */
#ifdef FLECS_DEPRECATED
#include "flecs/addons/deprecated.h"
#endif


/**
 * @defgroup type_roles Type Roles
 * @{
 */

/* Type roles are used to indicate the role of an entity in a type. If no flag
 * is specified, the entity is interpreted as a regular component or tag. Flags
 * are added to an entity by using a bitwise OR (|). */

/** Role bit added to roles to differentiate between roles and generations */
#define ECS_ROLE (1ull << 63)

/** Cases are used to switch between mutually exclusive components */
FLECS_API extern const ecs_id_t ECS_CASE;

/** Switches allow for fast switching between mutually exclusive components */
FLECS_API extern const ecs_id_t ECS_SWITCH;

/** The PAIR role indicates that the entity is a pair identifier. */
FLECS_API extern const ecs_id_t ECS_PAIR;

/** Enforce ownership of a component */
FLECS_API extern const ecs_id_t ECS_OVERRIDE;

/** Track whether component is enabled or not */
FLECS_API extern const ecs_id_t ECS_DISABLED;

/** @} */


/**
 * @defgroup builtin_tags Builtin Tags
 * @{
 */

/** Root scope for builtin flecs entities */
FLECS_API extern const ecs_entity_t EcsFlecs;

/* Core module scope */
FLECS_API extern const ecs_entity_t EcsFlecsCore;

/* Entity associated with world (used for "attaching" components to world) */
FLECS_API extern const ecs_entity_t EcsWorld;

/* Wildcard entity ("*"), Used in expressions to indicate wildcard matching */
FLECS_API extern const ecs_entity_t EcsWildcard;

/* This entity (".", "This"). Used in expressions to indicate This entity */
FLECS_API extern const ecs_entity_t EcsThis;

/* Can be added to relation to indicate it is transitive. */
FLECS_API extern const ecs_entity_t EcsTransitive;

/* Can be added to transitive relation to indicate it is inclusive. */
FLECS_API extern const ecs_entity_t EcsInclusive;

/* Can be added to component/relation to indicate it is final. Final components/
 * relations cannot be derived from using an IsA relationship. Queries will not
 * attempt to substitute a component/relationship with IsA subsets if they are
 * final. */
FLECS_API extern const ecs_entity_t EcsFinal;

/* Can be added to relation to indicate that it should never hold data, even
 * when it or the relation object is a component. */
FLECS_API extern const ecs_entity_t EcsTag;

/* Tag to indicate name identifier */
FLECS_API extern const ecs_entity_t EcsName;

/* Tag to indicate symbol identifier */
FLECS_API extern const ecs_entity_t EcsSymbol;

/* Used to express parent-child relations. */
FLECS_API extern const ecs_entity_t EcsChildOf;

/* Used to express is-a relations. An IsA relation indicates that the subject is
 * a subset of the relation object. For example:
 *   ecs_add_pair(world, Freighter, EcsIsA, SpaceShip);
 *
 * Here the Freighter is considered a subset of SpaceShip, meaning that every
 * entity that has Freighter also implicitly has SpaceShip.
 *
 * The subject of the relation (Freighter) inherits all components from any IsA
 * object (SpaceShip). If SpaceShip has a component "MaxSpeed", this component
 * will also appear on Freighter after adding (IsA, SpaceShip) to Freighter.
 *
 * The IsA relation is transitive. This means that if SpaceShip IsA Machine, 
 * then Freigther is also a Machine. As a result, Freighter also inherits all
 * components from Machine, just as it does from SpaceShip.
 *
 * Queries/filters may implicitly substitute predicates, subjects and objects 
 * with their IsA super/subsets. This behavior can be controlled by the "set" 
 * member of a query term.
 */
FLECS_API extern const ecs_entity_t EcsIsA;

/* Tag added to module entities */
FLECS_API extern const ecs_entity_t EcsModule;

/* Tag added to prefab entities. Any entity with this tag is automatically
 * ignored by filters/queries, unless EcsPrefab is explicitly added. */
FLECS_API extern const ecs_entity_t EcsPrefab;

/* When this tag is added to an entity it is skipped by all queries/filters */
FLECS_API extern const ecs_entity_t EcsDisabled;

/* Tag added to builtin/framework entites. This tag can be used to automatically
 * hide components/systems that are part of infrastructure code vs. application
 * code. The tag has no functional implications. */
FLECS_API extern const ecs_entity_t EcsHidden;

/* Event. Triggers when an id (component, tag, pair) is added to an entity */
FLECS_API extern const ecs_entity_t EcsOnAdd;

/* Event. Triggers when an id (component, tag, pair) is removed from an entity */
FLECS_API extern const ecs_entity_t EcsOnRemove;

/* Event. Triggers when a component is set for an entity */
FLECS_API extern const ecs_entity_t EcsOnSet;

/* Event. Triggers when a component is unset for an entity */
FLECS_API extern const ecs_entity_t EcsUnSet;

/* Event. Exactly-once trigger for when an entity matches/unmatches a filter */
FLECS_API extern const ecs_entity_t EcsMonitor;

/* Event. Triggers when an entity is deleted.
 * Also used as relation for defining cleanup behavior, see: 
 * https://github.com/SanderMertens/flecs/blob/master/docs/Relations.md#relation-cleanup-properties
 */
FLECS_API extern const ecs_entity_t EcsOnDelete;

/* Event. Triggers when a table is created. */
// FLECS_API extern const ecs_entity_t EcsOnCreateTable;

/* Event. Triggers when a table is deleted. */
// FLECS_API extern const ecs_entity_t EcsOnDeleteTable;

/* Event. Triggers when a table becomes empty (doesn't trigger on creation). */
FLECS_API extern const ecs_entity_t EcsOnTableEmpty;

/* Event. Triggers when a table becomes non-empty. */
FLECS_API extern const ecs_entity_t EcsOnTableNonEmpty;

/* Event. Triggers when a trigger is created. */
// FLECS_API extern const ecs_entity_t EcsOnCreateTrigger;

/* Event. Triggers when a trigger is deleted. */
// FLECS_API extern const ecs_entity_t EcsOnDeleteTrigger;

/* Event. Triggers when observable is deleted. */
// FLECS_API extern const ecs_entity_t EcsOnDeleteObservable;

/* Event. Triggers when lifecycle methods for a component are registered */
// FLECS_API extern const ecs_entity_t EcsOnComponentLifecycle;

/* Relationship used to define what should happen when an entity is deleted that
 * is added to other entities. For details see: 
 * https://github.com/SanderMertens/flecs/blob/master/docs/Relations.md#relation-cleanup-properties
 */
FLECS_API extern const ecs_entity_t EcsOnDeleteObject;

/* Specifies that a component/relation/object of relation should be removed when
 * it is deleted. Must be combined with EcsOnDelete or EcsOnDeleteObject. */
FLECS_API extern const ecs_entity_t EcsRemove;

/* Specifies that entities with a component/relation/object of relation should 
 * be deleted when the component/relation/object of relation is deleted. Must be 
 * combined with EcsOnDelete or EcsOnDeleteObject. */
FLECS_API extern const ecs_entity_t EcsDelete;

/* Specifies that whenever a component/relation/object of relation is deleted an
 * error should be thrown. Must be combined with EcsOnDelete or 
 * EcsOnDeleteObject. */
FLECS_API extern const ecs_entity_t EcsThrow;

/* System module tags */
FLECS_API extern const ecs_entity_t EcsDisabledIntern;
FLECS_API extern const ecs_entity_t EcsInactive;

/* Pipeline module tags */
FLECS_API extern const ecs_entity_t EcsPipeline;
FLECS_API extern const ecs_entity_t EcsPreFrame;
FLECS_API extern const ecs_entity_t EcsOnLoad;
FLECS_API extern const ecs_entity_t EcsPostLoad;
FLECS_API extern const ecs_entity_t EcsPreUpdate;
FLECS_API extern const ecs_entity_t EcsOnUpdate;
FLECS_API extern const ecs_entity_t EcsOnValidate;
FLECS_API extern const ecs_entity_t EcsPostUpdate;
FLECS_API extern const ecs_entity_t EcsPreStore;
FLECS_API extern const ecs_entity_t EcsOnStore;
FLECS_API extern const ecs_entity_t EcsPostFrame;

/* Value used to quickly check if component is builtin. This is used to quickly
 * filter out tables with builtin components (for example for ecs_delete) */
#define EcsLastInternalComponentId (ecs_id(EcsSystem))

/* The first user-defined component starts from this id. Ids up to this number
 * are reserved for builtin components */
#define EcsFirstUserComponentId (32)

/* The first user-defined entity starts from this id. Ids up to this number
 * are reserved for builtin components */
#define EcsFirstUserEntityId (ECS_HI_COMPONENT_ID + 128)

/** @} */


/**
 * @defgroup world_api World API
 * @{
 */

/** Create a new world.
 * A world manages all the ECS data and supporting infrastructure. Applications 
 * must have at least one world. Entities, component and system handles are 
 * local to a world and should not be shared between worlds.
 * 
 * This operation creates a world with all builtin modules loaded. 
 *
 * @return A new world object
 */
FLECS_API
ecs_world_t* ecs_init(void);

/** Same as ecs_init, but with minimal set of modules loaded.
 *
 * @return A new world object
 */
FLECS_API
ecs_world_t* ecs_mini(void);

/** Create a new world with arguments.
 * Same as ecs_init, but allows passing in command line arguments. These can be
 * used to dynamically enable flecs features to an application. Currently these
 * arguments are not used.
 *
 * @return A new world object
 */
FLECS_API
ecs_world_t* ecs_init_w_args(
    int argc,
    char *argv[]);

/** Delete a world.
 * This operation deletes the world, and everything it contains.
 *
 * @param world The world to delete.
 * @return Zero if successful, non-zero if failed.
 */
FLECS_API
int ecs_fini(
    ecs_world_t *world);

/** Register action to be executed when world is destroyed.
 * Fini actions are typically used when a module needs to clean up before a
 * world shuts down.
 * 
 * @param world The world.
 * @param action The function to execute.
 * @param ctx Userdata to pass to the function */
FLECS_API
void ecs_atfini(
    ecs_world_t *world,
    ecs_fini_action_t action,
    void *ctx);

/** Register action to be executed once after frame.
 * Post frame actions are typically used for calling operations that cannot be
 * invoked during iteration, such as changing the number of threads.
 * 
 * @param world The world.
 * @param action The function to execute.
 * @param ctx Userdata to pass to the function */
FLECS_API
void ecs_run_post_frame(
    ecs_world_t *world,
    ecs_fini_action_t action,
    void *ctx);    

/** Signal exit
 * This operation signals that the application should quit. It will cause
 * ecs_progress to return false.
 *
 * @param world The world to quit.
 */
FLECS_API
void ecs_quit(
    ecs_world_t *world);

/** Return whether a quit has been signaled.
 *
 * @param world The world.
 */
FLECS_API 
bool ecs_should_quit(
    const ecs_world_t *world);

/** Register ctor, dtor, copy & move actions for component.
 *
 * @param world The world.
 * @param component The component id for which to register the actions
 * @param actions Type that contains the component actions.
 */
FLECS_API
void ecs_set_component_actions_w_id(
    ecs_world_t *world,
    ecs_id_t id,
    EcsComponentLifecycle *actions);

/** Set a world context.
 * This operation allows an application to register custom data with a world
 * that can be accessed anywhere where the application has the world object.
 *
 * @param world The world.
 * @param ctx A pointer to a user defined structure.
 */
FLECS_API
void ecs_set_context(
    ecs_world_t *world,
    void *ctx);

/** Get the world context.
 * This operation retrieves a previously set world context.
 *
 * @param world The world.
 * @return The context set with ecs_set_context. If no context was set, the
 *         function returns NULL.
 */
FLECS_API
void* ecs_get_context(
    const ecs_world_t *world);

/** Get world info.
 *
 * @param world The world.
 * @return Pointer to the world info. This pointer will remain valid for as long
 *         as the world is valid.
 */
FLECS_API
const ecs_world_info_t* ecs_get_world_info(
    const ecs_world_t *world);

/** Dimension the world for a specified number of entities.
 * This operation will preallocate memory in the world for the specified number
 * of entities. Specifying a number lower than the current number of entities in
 * the world will have no effect.
 *
 * @param world The world.
 * @param entity_count The number of entities to preallocate.
 */
FLECS_API
void ecs_dim(
    ecs_world_t *world,
    int32_t entity_count);

/** Set a range for issueing new entity ids.
 * This function constrains the entity identifiers returned by ecs_new to the 
 * specified range. This operation can be used to ensure that multiple processes
 * can run in the same simulation without requiring a central service that
 * coordinates issueing identifiers.
 * 
 * If id_end is set to 0, the range is infinite. If id_end is set to a non-zero
 * value, it has to be larger than id_start. If id_end is set and ecs_new is
 * invoked after an id is issued that is equal to id_end, the application will
 * abort.
 * 
 * @param world The world.
 * @param id_start The start of the range.
 * @param id_end The end of the range.
 */
FLECS_API
void ecs_set_entity_range(
    ecs_world_t *world,
    ecs_entity_t id_start,
    ecs_entity_t id_end);

/** Enable/disable range limits.
 * When an application is both a receiver of range-limited entities and a
 * producer of range-limited entities, range checking needs to be temporarily
 * disabled when inserting received entities. Range checking is disabled on a 
 * stage, so setting this value is thread safe.
 *
 * @param world The world.
 * @param enable True if range checking should be enabled, false to disable.
 * @return The previous value.
 */
FLECS_API
bool ecs_enable_range_check(
    ecs_world_t *world,
    bool enable);

/** Enable world locking while in progress.
 * When locking is enabled, Flecs will lock the world while in progress. This
 * allows applications to interact with the world from other threads without
 * running into race conditions.
 *
 * This is a better alternative to applications putting a lock around calls to
 * ecs_progress, since ecs_progress can sleep when FPS control is enabled,
 * which is time during which other threads could perform work.
 *
 * Locking must be enabled before applications can use the ecs_lock and
 * ecs_unlock functions. Locking is turned off by default.
 *
 * @param world The world.
 * @param enable True if locking is to be enabled.
 * @result The previous value of the setting.
 */
FLECS_API
bool ecs_enable_locking(
    ecs_world_t *world,
    bool enable);

/** Locks the world.
 * See ecs_enable_locking for details.
 *
 * @param world The world.
 */
FLECS_API
void ecs_lock(
    ecs_world_t *world);

/** Unlocks the world.
 * See ecs_enable_locking for details.
 * 
 * @param world The world.
 */
FLECS_API
void ecs_unlock(
    ecs_world_t *world);

/** Wait until world becomes available.
 * When a non-flecs thread needs to interact with the world, it should invoke
 * this function to wait until the world becomes available (as in, it is not
 * progressing the frame). Invoking this function guarantees that the thread
 * will not starve. (as opposed to simply taking the world lock).
 *
 * An application will have to invoke ecs_end_wait after this function returns.
 * 
 * @param world The world.
 */
FLECS_API 
void ecs_begin_wait(
    ecs_world_t *world);

/** Release world after calling ecs_begin_wait.
 * This operation should be invoked after invoking ecs_begin_wait, and will
 * release the world back to the thread running the main loop.
 *
 * @param world The world.
 */
FLECS_API 
void ecs_end_wait(
    ecs_world_t *world);

/** Enable or disable tracing.
 * This will enable builtin tracing. For tracing to work, it will have to be
 * compiled in which requires defining one of the following macro's:
 *
 * ECS_TRACE_0 - All tracing is disabled
 * ECS_TRACE_1 - Enable tracing level 1
 * ECS_TRACE_2 - Enable tracing level 2 and below
 * ECS_TRACE_3 - Enable tracing level 3 and below
 *
 * If no tracing level is defined and this is a debug build, ECS_TRACE_3 will
 * have been automatically defined.
 *
 * The provided level corresponds with the tracing level. If -1 is provided as
 * value, warnings are disabled. If -2 is provided, errors are disabled as well.
 *
 * @param level Desired tracing level.
 */
FLECS_API
void ecs_tracing_enable(
    int level);

/** Enable/disable tracing with colors.
 * By default colors are enabled.
 *
 * @param enabled Whether to enable tracing with colors.
 */
FLECS_API
void ecs_tracing_color_enable(
    bool enabled);

/** Measure frame time. 
 * Frame time measurements measure the total time passed in a single frame, and 
 * how much of that time was spent on systems and on merging.
 *
 * Frame time measurements add a small constant-time overhead to an application.
 * When an application sets a target FPS, frame time measurements are enabled by
 * default.
 *
 * @param world The world.
 * @param enable Whether to enable or disable frame time measuring.
 */
FLECS_API void ecs_measure_frame_time(
    ecs_world_t *world,
    bool enable);

/** Measure system time. 
 * System time measurements measure the time spent in each system.
 *
 * System time measurements add overhead to every system invocation and 
 * therefore have a small but measurable impact on application performance.
 * System time measurements must be enabled before obtaining system statistics.
 *
 * @param world The world.
 * @param enable Whether to enable or disable system time measuring.
 */
FLECS_API void ecs_measure_system_time(
    ecs_world_t *world,
    bool enable);   

/** Set target frames per second (FPS) for application.
 * Setting the target FPS ensures that ecs_progress is not invoked faster than
 * the specified FPS. When enabled, ecs_progress tracks the time passed since
 * the last invocation, and sleeps the remaining time of the frame (if any).
 *
 * This feature ensures systems are ran at a consistent interval, as well as
 * conserving CPU time by not running systems more often than required.
 *
 * Note that ecs_progress only sleeps if there is time left in the frame. Both
 * time spent in flecs as time spent outside of flecs are taken into
 * account.
 *
 * @param world The world.
 * @param fps The target FPS.
 */
FLECS_API
void ecs_set_target_fps(
    ecs_world_t *world,
    FLECS_FLOAT fps);     

/** Get current number of threads. */
FLECS_API
int32_t ecs_get_threads(
    ecs_world_t *world);

/** @} */

/**
 * @defgroup creating_entities Creating Entities
 * @{
 */

/** Create new entity id.
 * This operation returns an unused entity id.
 *
 * @param world The world.
 * @return The new entity id.
 */
FLECS_API
ecs_entity_t ecs_new_id(
    ecs_world_t *world);

/** Create new low id.
 * This operation returns a new low id. Entity ids start after the
 * ECS_HI_COMPONENT_ID constant. This reserves a range of low ids for things 
 * like components, and allows parts of the code to optimize operations.
 *
 * Note that ECS_HI_COMPONENT_ID does not represent the maximum number of 
 * components that can be created, only the maximum number of components that
 * can take advantage of these optimizations.
 * 
 * This operation does not recycle ids.
 *
 * @param world The world.
 * @return The new component id.
 */
FLECS_API
ecs_entity_t ecs_new_low_id(
    ecs_world_t *world);

/** Create new entity.
 * This operation creates a new entity with a single entity in its type. The
 * entity may contain type roles. This operation recycles ids.
 *
 * @param world The world.
 * @param entity The entity to initialize the new entity with.
 * @return The new entity.
 */
FLECS_API
ecs_entity_t ecs_new_w_id(
    ecs_world_t *world,
    ecs_id_t id);

/** Find or create an entity. 
 * This operation creates a new entity, or modifies an existing one. When a name
 * is set in the ecs_entity_desc_t::name field and ecs_entity_desc_t::entity is
 * not set, the operation will first attempt to find an existing entity by that
 * name. If no entity with that name can be found, it will be created.
 *
 * If both a name and entity handle are provided, the operation will check if
 * the entity name matches with the provided name. If the names do not match,
 * the function will fail and return 0.
 *
 * If an id to a non-existing entity is provided, that entity id become alive.
 * 
 * See the documentation of ecs_entity_desc_t for more details. 
 *
 * @param world The world.
 * @param desc Entity init parameters.
 * @return A handle to the new or existing entity, or 0 if failed.
 */
FLECS_API
ecs_entity_t ecs_entity_init(
    ecs_world_t *world,
    const ecs_entity_desc_t *desc);

/** Find or create a component. 
 * This operation creates a new component, or finds an existing one. The find or
 * create behavior is the same as ecs_entity_init.
 *
 * When an existing component is found, the size and alignment are verified with
 * the provided values. If the values do not match, the operation will fail.
 *
 * See the documentation of ecs_component_desc_t for more details. 
 *
 * @param world The world.
 * @param desc Component init parameters.
 * @return A handle to the new or existing component, or 0 if failed.
 */
FLECS_API
ecs_entity_t ecs_component_init(
    ecs_world_t *world,
    const ecs_component_desc_t *desc);

/** Create a new type entity. 
 * This operation creates a new type entity, or finds an existing one. The find 
 * or create behavior is the same as ecs_entity_init.
 *
 * A type entity is an entity with the EcsType component. This component
 * a pointer to an ecs_type_t, which allows for the creation of named types.
 * Named types are used in a few places, such as for pipelines and filter terms
 * with the EcsAndFrom or EcsOrFrom operators.
 *
 * When an existing type entity is found, its types are verified with the
 * provided values. If the values do not match, the operation will fail.
 *
 * See the documentation of ecs_type_desc_t for more details.
 *
 * @param world The world.
 * @param desc Type entity init parameters.
 * @return A handle to the new or existing type, or 0 if failed.
*/
FLECS_API
ecs_entity_t ecs_type_init(
    ecs_world_t *world,
    const ecs_type_desc_t *desc);    

/** Create N new entities.
 * This operation is the same as ecs_new_w_id, but creates N entities
 * instead of one and does not recycle ids.
 * 
 * @param world The world.
 * @param entity The entity.
 * @param count The number of entities to create.
 * @return The first entity id of the newly created entities.
 */
FLECS_API
const ecs_entity_t* ecs_bulk_new_w_id(
    ecs_world_t *world,
    ecs_id_t id,
    int32_t count);

/** Clone an entity
 * This operation clones the components of one entity into another entity. If
 * no destination entity is provided, a new entity will be created. Component
 * values are not copied unless copy_value is true.
 *
 * @param world The world.
 * @param dst The entity to copy the components to.
 * @param src The entity to copy the components from.
 * @param copy_value If true, the value of components will be copied to dst.
 * @return The destination entity.
 */
FLECS_API
ecs_entity_t ecs_clone(
    ecs_world_t *world,
    ecs_entity_t dst,
    ecs_entity_t src,
    bool copy_value);

/** @} */

/**
 * @defgroup adding_removing Adding & Removing
 * @{
 */

/** Add an entity to an entity.
 * This operation adds a single entity to the type of an entity. Type roles may
 * be used in combination with the added entity. If the entity already has the
 * entity, this operation will have no side effects.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The id to add.
 */
FLECS_API
void ecs_add_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);

/** Remove an entity from an entity.
 * This operation removes a single entity from the type of an entity. Type roles
 * may be used in combination with the added entity. If the entity does not have
 * the entity, this operation will have no side effects.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The id to remove.
 */
FLECS_API
void ecs_remove_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);

/** @} */


/**
 * @defgroup enabling_disabling Enabling & Disabling components.
 * @{
 */

/** Enable or disable component.
 * Enabling or disabling a component does not add or remove a component from an
 * entity, but prevents it from being matched with queries. This operation can
 * be useful when a component must be temporarily disabled without destroying
 * its value. It is also a more performant operation for when an application
 * needs to add/remove components at high frequency, as enabling/disabling is
 * cheaper than a regular add or remove.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The component.
 * @param enable True to enable the component, false to disable.
 */
FLECS_API 
void ecs_enable_component_w_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id,
    bool enable);

/** Test if component is enabled.
 * Test whether a component is currently enabled or disabled. This operation
 * will return true when the entity has the component and if it has not been
 * disabled by ecs_enable_component.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The component.
 * @return True if the component is enabled, otherwise false.
 */
FLECS_API 
bool ecs_is_component_enabled_w_id(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);

/** @} */


/**
 * @defgroup pairs Pairs
 * @{
 */
 
/** Make a pair identifier.
 * This function is equivalent to using the ecs_pair macro, and is added for
 * convenience to make it easier for non C/C++ bindings to work with pairs.
 *
 * @param relation The relation of the pair.
 * @param object The object of the pair.
 */
FLECS_API
ecs_id_t ecs_make_pair(
    ecs_entity_t relation,
    ecs_entity_t object);

/** @} */


/**
 * @defgroup deleting Deleting Entities and components
 * @{
 */

/** Clear all components.
 * This operation will clear all components from an entity but will not delete
 * the entity itself. This effectively prevents the entity id from being 
 * recycled.
 *
 * @param world The world.
 * @param entity The entity.
 */
FLECS_API
void ecs_clear(
    ecs_world_t *world,
    ecs_entity_t entity);

/** Delete an entity.
 * This operation will delete an entity and all of its components. The entity id
 * will be recycled. Repeatedly calling ecs_delete without ecs_new or
 * ecs_new_w_id will cause a memory leak as it will cause
 * the list with ids that can be recycled to grow unbounded.
 *
 * @param world The world.
 * @param entity The entity.
 */
FLECS_API
void ecs_delete(
    ecs_world_t *world,
    ecs_entity_t entity);


/** Delete children of an entity.
 * This operation deletes all children of a parent entity. If a parent has no
 * children this operation has no effect.
 *
 * @param world The world.
 * @param parent The parent entity.
 */
FLECS_API
void ecs_delete_children(
    ecs_world_t *world,
    ecs_entity_t parent);

/** @} */


/**
 * @defgroup getting Getting Components
 * @{
 */

/** Get an immutable pointer to a component.
 * This operation obtains a const pointer to the requested component. The
 * operation accepts the component entity id.
 *
 * @param world The world.
 * @param entity The entity.
 * @param component The entity id of the component to obtain.
 * @return The component pointer, NULL if the entity does not have the component.
 */
FLECS_API
const void* ecs_get_id(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);


/** Get an immutable reference to a component.
 * This operation is similar to ecs_get_id but it stores temporary
 * information in a `ecs_ref_t` value which allows subsequent lookups to be
 * faster.
 *
 * @param world The world.
 * @param ref Pointer to a ecs_ref_t value. Must be initialized.
 * @param entity The entity.
 * @param component The entity id of the component to obtain.
 * @return The component pointer, NULL if the entity does not have the component.
 */
FLECS_API
const void* ecs_get_ref_w_id(
    const ecs_world_t *world,
    ecs_ref_t *ref,
    ecs_entity_t entity,
    ecs_id_t id);

/** Get case for switch.
 * This operation gets the current case for the specified switch. If the current
 * switch is not set for the entity, the operation will return 0.
 *
 * @param world The world.
 * @param e The entity.
 * @param sw The switch for which to obtain the case.
 * @return The current case for the specified switch. 
 */
FLECS_API
ecs_entity_t ecs_get_case(
    const ecs_world_t *world,
    ecs_entity_t e,
    ecs_entity_t sw);

/** @} */


/**
 * @defgroup setting Setting Components
 * @{
 */

/** Get a mutable pointer to a component.
 * This operation is similar to ecs_get_id but it returns a mutable 
 * pointer. If this operation is invoked from inside a system, the entity will
 * be staged and a pointer to the staged component will be returned.
 *
 * If the entity did not yet have the component, the component will be added by
 * this operation. In this case the is_added out parameter will be set to true.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The entity id of the component to obtain.
 * @param is_added Out parameter that returns true if the component was added.
 * @return The component pointer.
 */
FLECS_API
void* ecs_get_mut_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id,
    bool *is_added); 

/** Emplace a component.
 * Emplace is similar to get_mut except that the component constructor is not
 * invoked for the returned pointer, allowing the component to be "constructed"
 * directly in the storage.
 *
 * Emplace can only be used if the entity does not yet have the component. If
 * the entity has the component, the operation will fail.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The component to obtain.
 * @return The (uninitialized) component pointer.
 */
FLECS_API
void* ecs_emplace_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id); 

/** Signal that a component has been modified.
 * This operation allows an application to signal to Flecs that a component has
 * been modified. As a result, OnSet systems will be invoked.
 *
 * This operation is commonly used together with ecs_get_mut.
 *
 * @param world The world.
 * @param entity The entity.
 * @param component The entity id of the component that was modified.
 */
FLECS_API 
void ecs_modified_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);

/** Set the value of a component.
 * This operation allows an application to set the value of a component. The
 * operation is equivalent to calling ecs_get_mut and ecs_modified.
 *
 * If the provided entity is 0, a new entity will be created.
 *
 * @param world The world.
 * @param entity The entity.
 * @param component The entity id of the component to set.
 * @param size The size of the pointer to the value.
 * @param ptr The pointer to the value.
 * @return The entity. A new entity if no entity was provided.
 */
FLECS_API
ecs_entity_t ecs_set_id(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id,
    size_t size,
    const void *ptr);

/** @} */


/**
 * @defgroup metadata Entity Metadata
 * @{
 */

/** Test whether an entity is valid.
 * Entities that are valid can be used with API functions.
 *
 * An entity is valid if it is not 0 and if it is alive. If the provided id has
 * a role or a pair, the contents of the role or the pair will be checked for
 * validity.
 *
 * is_valid will return true for ids that don't exist (alive or not alive). This
 * allows for using ids that have never been created by ecs_new or similar. In
 * this the function differs from ecs_is_alive, which will return false for
 * entities that do not yet exist.
 *
 * The operation will return false for an id that exists and is not alive, as
 * using this id with an API operation would cause it to assert.
 *
 * @param world The world.
 * @param e The entity.
 * @return True if the entity is valid, false if the entity is not valid.
 */
FLECS_API
bool ecs_is_valid(
    const ecs_world_t *world,
    ecs_entity_t e);

/** Test whether an entity is alive.
 * An entity is alive when it has been returned by ecs_new (or similar) or if
 * it is not empty (componentts have been explicitly added to the id).
 *
 * @param world The world.
 * @param e The entity.
 * @return True if the entity is alive, false if the entity is not alive.
 */
FLECS_API
bool ecs_is_alive(
    const ecs_world_t *world,
    ecs_entity_t e);

/** Remove generation from entity id.
 *
 * @param e The entity id.
 * @return The entity id without the generation count.
 */
FLECS_API
ecs_id_t ecs_strip_generation(
    ecs_entity_t e);

/** Get alive identifier.
 * In some cases an application may need to work with identifiers from which
 * the generation has been stripped. A typical scenario in which this happens is
 * when iterating relationships in an entity type.
 *
 * For example, when obtaining the parent id from a ChildOf relation, the parent
 * (object part of the pair) will have been stored in a 32 bit value, which 
 * cannot store the entity generation. This function can retrieve the identifier
 * with the current generation for that id.
 *
 * If the provided identifier is not alive, the function will return 0.
 *
 * @param world The world.
 * @param e The for which to obtain the current alive entity id.
 * @return The alive entity id if there is one, or 0 if the id is not alive.
 */
FLECS_API
ecs_entity_t ecs_get_alive(
    const ecs_world_t *world,
    ecs_entity_t e);

/** Ensure id is alive.
 * This operation ensures that the provided id is alive. This is useful in
 * scenarios where an application has an existing id that has not been created
 * with ecs_new (such as a global constant or an id from a remote application).
 *
 * Before this operation the id must either not yet exist, or must exist with
 * the same generation as the provided id. If the id has been recycled and the
 * provided id does not have the same generation count, the function will fail.
 *
 * If the provided entity is not alive, and the provided generation count is
 * equal to the current generation (which is the future generation when the id
 * will be recycled) the id will become alive again.
 *
 * If the provided id has a non-zero generation count and the id does not exist
 * in the world, the id will be created with the specified generation.
 *
 * This behavior ensures that an application can use ecs_ensure to track the
 * lifecycle of an id without explicitly having to create it. It also protects
 * against reviving an id with a generation count that was not yet due.
 *
 * @param world The world.
 * @param entity The entity id to make alive.
 */
FLECS_API
void ecs_ensure(
    ecs_world_t *world,
    ecs_entity_t e);

/** Test whether an entity exists.
 * Similar as ecs_is_alive, but ignores entity generation count.
 *
 * @param world The world.
 * @param e The entity.
 * @return True if the entity exists, false if the entity does not exist.
 */
FLECS_API
bool ecs_exists(
    const ecs_world_t *world,
    ecs_entity_t e);

/** Get the type of an entity.
 *
 * @param world The world.
 * @param entity The entity.
 * @return The type of the entity, NULL if the entity has no components.
 */
FLECS_API
ecs_type_t ecs_get_type(
    const ecs_world_t *world,
    ecs_entity_t entity);

/** Get the table of an entity.
 *
 * @param world The world.
 * @param entity The entity.
 * @return The table of the entity, NULL if the entity has no components.
 */
FLECS_API
ecs_table_t* ecs_get_table(
    const ecs_world_t *world,
    ecs_entity_t entity);

/** Get the typeid of an entity.
 *
 * @param world The world.
 * @param entity The entity.
 * @return The typeid of the entity.
 */
FLECS_API
ecs_entity_t ecs_get_typeid(
    const ecs_world_t *world,
    ecs_id_t e);

/** Get the name of an entity.
 * This will return the name as specified in the EcsName component.
 *
 * @param world The world.
 * @param entity The entity.
 * @return The type of the entity, NULL if the entity has no name.
 */
FLECS_API
const char* ecs_get_name(
    const ecs_world_t *world,
    ecs_entity_t entity);

/** Get the symbol of an entity.
 * This will return the name as specified in the EcsSymbol component.
 *
 * @param world The world.
 * @param entity The entity.
 * @return The type of the entity, NULL if the entity has no name.
 */
FLECS_API
const char* ecs_get_symbol(
    const ecs_world_t *world,
    ecs_entity_t entity);

/** Set the name of an entity.
 * This will set or overwrite the name of an entity. If no entity is provided,
 * a new entity will be created.
 *
 * The name will be stored in the EcsName component.
 *
 * @param world The world.
 * @param entity The entity.
 * @param name The entity's name.
 * @return The provided entity, or a new entity if 0 was provided.
 */
FLECS_API
ecs_entity_t ecs_set_name(
    ecs_world_t *world,
    ecs_entity_t entity,
    const char *name);

/** Set the symbol of an entity.
 * This will set or overwrite the symbol of an entity. If no entity is provided,
 * a new entity will be created.
 *
 * The symbol will be stored in the EcsName component.
 *
 * @param world The world.
 * @param entity The entity.
 * @param symbol The entity's symbol.
 * @return The provided entity, or a new entity if 0 was provided.
 */
FLECS_API
ecs_entity_t ecs_set_symbol(
    ecs_world_t *world,
    ecs_entity_t entity,
    const char *symbol);

/** Convert type role to string.
 * This operation converts a type role to a string.
 * 
 * @param world The world.
 * @param entity The entity containing the type role.
 * @return The type role string, or NULL if no type role is provided.
 */
FLECS_API
const char* ecs_role_str(
    ecs_entity_t entity);

/** Convert id to string.
 * This operation interprets the structure of an id and converts it to a string.
 *
 * @param world The world.
 * @param id The id to convert to a string.
 * @param buffer The buffer in which to store the string.
 * @param buffer_len The length of the provided buffer.
 * @return The number of characters required to write the string.
 */
FLECS_API
char* ecs_id_str(
    const ecs_world_t *world,
    ecs_id_t id);

FLECS_API
size_t ecs_id_str_w_buf(
    const ecs_world_t *world,
    ecs_id_t id,
    char *buffer,
    size_t buffer_len);

/** Test if an entity has an entity.
 * This operation returns true if the entity has the provided entity in its 
 * type.
 *
 * @param world The world.
 * @param entity The entity.
 * @param id The id to test for.
 * @return True if the entity has the entity, false if not.
 */
FLECS_API
bool ecs_has_id(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_id_t id);

/** Get the object of a relation.
 * This will return a object of the entity for the specified relation. The index
 * allows for iterating through the objects, if a single entity has multiple
 * objects for the same relation.
 *
 * If the index is larger than the total number of instances the entity has for
 * the relation, the operation will return 0.
 *
 * @param world The world.
 * @param entity The entity.
 * @param rel The relation between the entity and the object.
 * @param index The index of the relation instance.
 * @return The object for the relation at the specified index.
 */
FLECS_API
ecs_entity_t ecs_get_object(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t rel,
    int32_t index);

/** Get the object of a relation for a given id.
 * This operation returns the first entity that has the provided id by following
 * the specified relationship. If the entity itself has the id then entity will
 * be returned. If the id cannot be found on the entity or by following the
 * relation, the operation will return 0.
 * 
 * This operation can be used to lookup, for example, which prefab is providing
 * a component by specifying the IsA relation:
 * 
 *   // Is Position provided by the entity or one of its base entities?
 *   ecs_get_object_for_id(world, entity, EcsIsA, ecs_id(Position))
 * 
 * @param world The world.
 * @param entity The entity.
 * @param rel The relationship to follow.
 * @param id The id to lookup.
 * @return The entity for which the object has been found.
 */
FLECS_API
ecs_entity_t ecs_get_object_for_id(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t rel,
    ecs_id_t id);

/** Enable or disable an entity.
 * This operation enables or disables an entity by adding or removing the
 * EcsDisabled tag. A disabled entity will not be matched with any systems,
 * unless the system explicitly specifies the EcsDisabled tag.
 *
 * @param world The world.
 * @param entity The entity to enable or disable.
 * @param enabled true to enable the entity, false to disable.
 */
FLECS_API
void ecs_enable(
    ecs_world_t *world,
    ecs_entity_t entity,
    bool enabled);

/** Count entities that have the specified id.
 * Returns the number of entities that have the specified id.
 *
 * @param world The world.
 * @param entity The id to search for.
 * @return The number of entities that have the id.
 */
FLECS_API
int32_t ecs_count_id(
    const ecs_world_t *world,
    ecs_id_t entity);

/** @} */


/**
 * @defgroup lookup Lookups
 * @{
 */

/** Lookup an entity by name.
 * Returns an entity that matches the specified name. Only looks for entities in
 * the current scope (root if no scope is provided).
 *
 * @param world The world.
 * @param name The entity name.
 * @return The entity with the specified name, or 0 if no entity was found.
 */
FLECS_API
ecs_entity_t ecs_lookup(
    const ecs_world_t *world,
    const char *name);

/** Lookup a child entity by name.
 * Returns an entity that matches the specified name. Only looks for entities in
 * the provided parent. If no parent is provided, look in the current scope (
 * root if no scope is provided).
 *
 * @param world The world.
 * @param name The entity name.
 * @return The entity with the specified name, or 0 if no entity was found.
 */
FLECS_API
ecs_entity_t ecs_lookup_child(
    const ecs_world_t *world,
    ecs_entity_t parent,
    const char *name);

/** Lookup an entity from a path.
 * Lookup an entity from a provided path, relative to the provided parent. The
 * operation will use the provided separator to tokenize the path expression. If
 * the provided path contains the prefix, the search will start from the root.
 *
 * If the entity is not found in the provided parent, the operation will 
 * continue to search in the parent of the parent, until the root is reached. If
 * the entity is still not found, the lookup will search in the flecs.core
 * scope. If the entity is not found there either, the function returns 0.
 *
 * @param world The world.
 * @param parent The entity from which to resolve the path.
 * @param path The path to resolve.
 * @param sep The path separator.
 * @param prefix The path prefix.
 * @param recursive Recursively traverse up the tree until entity is found.
 * @return The entity if found, else 0.
 */
FLECS_API
ecs_entity_t ecs_lookup_path_w_sep(
    const ecs_world_t *world,
    ecs_entity_t parent,
    const char *path,
    const char *sep,
    const char *prefix,
    bool recursive);

/** Lookup an entity by its symbol name.
 * This looks up an entity by the symbol name that was provided in EcsName. The
 * operation does not take into account scoping, which means it will search all
 * entities that have an EcsName.
 *
 * This operation can be useful to resolve, for example, a type by its C 
 * identifier, which does not include the Flecs namespacing.
 */
FLECS_API
ecs_entity_t ecs_lookup_symbol(
    const ecs_world_t *world,
    const char *symbol,
    bool lookup_as_path);

/* Add alias for entity to global scope */
FLECS_API
void ecs_use(
    ecs_world_t *world,
    ecs_entity_t entity,
    const char *name);

/** @} */


/**
 * @defgroup paths Paths
 * @{
 */

/** Get a path identifier for an entity.
 * This operation creates a path that contains the names of the entities from
 * the specified parent to the provided entity, separated by the provided 
 * separator. If no parent is provided the path will be relative to the root. If
 * a prefix is provided, the path will be prefixed by the prefix.
 *
 * If the parent is equal to the provided child, the operation will return an
 * empty string. If a nonzero component is provided, the path will be created by 
 * looking for parents with that component.
 *
 * The returned path should be freed by the application.
 *
 * @param world The world.
 * @param parent The entity from which to create the path.
 * @param child The entity to which to create the path.
 * @return The relative entity path.
 */
FLECS_API
char* ecs_get_path_w_sep(
    const ecs_world_t *world,
    ecs_entity_t parent,
    ecs_entity_t child,
    const char *sep,
    const char *prefix);

/** Find or create entity from path.
 * This operation will find or create an entity from a path, and will create any
 * intermediate entities if required. If the entity already exists, no entities
 * will be created.
 *
 * If the path starts with the prefix, then the entity will be created from the
 * root scope.
 *
 * @param world The world.
 * @param parent The entity relative to which the entity should be created.
 * @param path The path to create the entity for.
 * @param sep The separator used in the path.
 * @param prefix The prefix used in the path.
 * @return The entity.
 */
FLECS_API
ecs_entity_t ecs_new_from_path_w_sep(
    ecs_world_t *world,
    ecs_entity_t parent,
    const char *path,
    const char *sep,
    const char *prefix);

/** Add specified path to entity.
 * This operation is similar to ecs_new_from_path, but will instead add the path
 * to an existing entity.
 *
 * If an entity already exists for the path, it will be returned instead.
 *
 * @param world The world.
 * @param entity The entity to which to add the path.
 * @param parent The entity relative to which the entity should be created.
 * @param path The path to create the entity for.
 * @param sep The separator used in the path.
 * @param prefix The prefix used in the path.
 * @return The entity.
 */ 
FLECS_API
ecs_entity_t ecs_add_path_w_sep(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t parent,
    const char *path,
    const char *sep,
    const char *prefix);

/** @} */


/**
 * @defgroup scopes Scopes
 * @{
 */

/** Set the current scope.
 * This operation sets the scope of the current stage to the provided entity.
 * As a result new entities will be created in this scope, and lookups will be
 * relative to the provided scope.
 *
 * It is considered good practice to restore the scope to the old value.
 *
 * @param world The world.
 * @param scope The entity to use as scope.
 * @return The previous scope.
 */
FLECS_API
ecs_entity_t ecs_set_scope(
    ecs_world_t *world,
    ecs_entity_t scope);

/** Get the current scope.
 * Get the scope set by ecs_set_scope. If no scope is set, this operation will
 * return 0.
 *
 * @param world The world.
 * @return The current scope.
 */
FLECS_API
ecs_entity_t ecs_get_scope(
    const ecs_world_t *world);

/** Set current with id.
 * New entities are automatically created with the specified id.
 *
 * @param world The world.
 * @param id The id.
 * @return The previous id.
 */
FLECS_API
ecs_entity_t ecs_set_with(
    ecs_world_t *world,
    ecs_id_t id);

/** Get current with id.
 * Get the id set with ecs_set_with.
 *
 * @param world The world.
 * @param id The id.
 * @return The previous id.
 */
FLECS_API
ecs_entity_t ecs_get_with(
    const ecs_world_t *world);

/** Set a name prefix for newly created entities.
 * This is a utility that lets C modules use prefixed names for C types and
 * C functions, while using names for the entity names that do not have the 
 * prefix. The name prefix is currently only used by ECS_COMPONENT.
 *
 * @param world The world.
 * @param prefix The name prefix to use.
 * @return The previous prefix.
 */
FLECS_API
const char* ecs_set_name_prefix(
    ecs_world_t *world,
    const char *prefix);    

/** @} */


/**
 * @defgroup terms Terms
 * @{
 */

/** Iterator for a single (component) id.
 * A term iterator returns all entities (tables) that match a single (component)
 * id. The search for the matching set of entities (tables) is performed in 
 * constant time.
 *
 * Currently only trivial terms are supported (see ecs_term_is_trivial). Only
 * the id field of the term needs to be initialized.
 *
 * @param world The world.
 * @param term The term.
 * @return The iterator.
 */
FLECS_API
ecs_iter_t ecs_term_iter(
    const ecs_world_t *world,
    ecs_term_t *term);

/** Progress the term iterator.
 * This operation progresses the term iterator to the next table. The 
 * iterator must have been initialized with `ecs_term_iter`. This operation 
 * must be invoked at least once before interpreting the contents of the 
 * iterator.
 *
 * @param iter The iterator.
 * @returns True if more data is available, false if not.
 */
FLECS_API
bool ecs_term_next(
    ecs_iter_t *it);

/** Test whether term id is set.
 *
 * @param id The term id.
 * @return True when set, false when not set.
 */
FLECS_API 
bool ecs_term_id_is_set(
    const ecs_term_id_t *id);

/** Test whether a term is set.
 * This operation can be used to test whether a term has been initialized with
 * values or whether it is empty.
 *
 * An application generally does not need to invoke this operation. It is useful
 * when initializing a 0-initialized array of terms (like in ecs_term_desc_t) as
 * this operation can be used to find the last initialized element.
 *
 * @param term The term.
 * @return True when set, false when not set.
 */
FLECS_API
bool ecs_term_is_initialized(
    const ecs_term_t *term);

/** Test whether a term is a trivial term.
 * A trivial term is a term that only contains a type id. Trivial terms must not
 * have read/write annotations, relation substitutions and subjects other than
 * 'This'. Examples of trivial terms are:
 * - 'Position'
 * - 'Position(This)'
 * - '(Likes, IceCream)'
 * - 'Likes(This, IceCream)'
 * 
 * Examples of non-trivial terms are:
 * - '[in] Position'
 * - 'Position(MyEntity)'
 * - 'Position(self|superset)'
 *
 * Trivial terms are useful in expressions that should just represent a list of
 * components, such as when parsing the list of components to add to an entity.
 *
 * The term passed to this operation must be finalized. Terms returned by the
 * parser are guaranteed to be finalized.
 *
 * @param term The term.
 * @return True if term is trivial, false if it is not.
 */
FLECS_API
bool ecs_term_is_trivial(
    const ecs_term_t *term);

/** Finalize term.
 * Ensure that all fields of a term are consistent and filled out. This 
 * operation should be invoked before using and after assigning members to, or 
 * parsing a term. When a term contains unresolved identifiers, this operation
 * will resolve and assign the identifiers. If the term contains any identifiers
 * that cannot be resolved, the operation will fail.
 *
 * An application generally does not need to invoke this operation as the APIs
 * that use terms (such as filters, queries and triggers) will finalize terms
 * when they are created.
 *
 * The name and expr parameters are optional, and only used for giving more 
 * descriptive error messages.
 *
 * @param world The world.
 * @param name The name of the entity that uses the term (such as a system).
 * @param expr The string expression of which the term is a part.
 * @param term The term to finalize.
 * @return Zero if success, nonzero if an error occurred.
 */
FLECS_API 
int ecs_term_finalize(
    const ecs_world_t *world,
    const char *name,
    ecs_term_t *term);

/** Copy resources of a term to another term.
 * This operation copies one term to another term. If the source term contains
 * allocated resources (such as identifiers), they will be duplicated so that
 * no memory is shared between the terms.
 *
 * @param dst The term to copy to.
 * @param src The term to copy from.
 */
FLECS_API 
ecs_term_t ecs_term_copy(
    const ecs_term_t *src);

/** Move resources of a term to another term.
 * Same as copy, but moves resources from src, if src->move is set to true. If
 * src->move is not set to true, this operation will do a copy.
 *
 * The conditional move reduces redundant allocations in scenarios where a list 
 * of terms is partially created with allocated resources.
 *
 * @param dst The term to copy to.
 * @param src The term to copy from.
 */
FLECS_API 
ecs_term_t ecs_term_move(
    ecs_term_t *src);    

/** Free resources of term.
 * This operation frees all resources (such as identifiers) of a term. The term
 * object itself is not freed.
 *
 * @param term The term to free.
 */
FLECS_API
void ecs_term_fini(
    ecs_term_t *term);

/** Utility to match an id with a pattern.
 * This operation returns true if the provided pattern matches the provided
 * id. The pattern may contain a wildcard (or wildcards, when a pair).
 *
 * @param id The id.
 * @param pattern The pattern to compare with.
 */
FLECS_API
bool ecs_id_match(
    ecs_id_t id,
    ecs_id_t pattern);

/** Utility to check if id is a pair.
 *
 * @param id The id.
 * @return True if id is a pair.
 */
FLECS_API
bool ecs_id_is_pair(
    ecs_id_t id);

/** Utility to check if id is a wildcard.
 *
 * @param id The id.
 * @return True if id is a wildcard or a pair containing a wildcard.
 */
FLECS_API
bool ecs_id_is_wildcard(
    ecs_id_t id);

/** @} */


/**
 * @defgroup filters Filters
 * @{
 */

/** Initialize filter 
 * A filter is a lightweight object that can be used to query for entities in
 * a world. Filters, as opposed to queries, do not cache results. They are 
 * therefore slower to iterate, but are faster to create.
 *
 * This operation will at minimum allocate an array to hold the filter terms in
 * the returned filter struct. It may allocate additional memory if the provided
 * description contains a name, expression, or if the provided array of terms
 * contains strings (identifier names or term names).
 *
 * It is possible to create a filter without allocating any memory, by setting
 * the "terms" and "term_count" members directly. When doing so an application
 * should not call ecs_filter_init but ecs_filter_finalize. This will ensure
 * that all fields are consistent and properly filled out.
 *
 * @param world The world.
 * @param desc Properties for the filter to create.
 * @param filter_out The filter.
 * @return Zero if successful, non-zero if failed.
 */
FLECS_API
int ecs_filter_init(
    const ecs_world_t *world,
    ecs_filter_t *filter_out,
    const ecs_filter_desc_t *desc);

/** Deinitialize filter.
 * Free resources associated with filter.
 *
 * @param filter The filter to deinitialize.
 */
FLECS_API
void ecs_filter_fini(
    ecs_filter_t *filter); 

/** Finalize filter.
 * When manually assigning an array of terms to the filter struct (so not when
 * using ecs_filter_init), this operation should be used to ensure that all 
 * terms are assigned properly and all (derived) fields have been set.
 *
 * When ecs_filter_init is used to create the filter, this function should not
 * be called. The purpose of this operation is to support creation of filters
 * without allocating memory.
 *
 * @param filter The filter to finalize.
 * @return Zero if filter is valid, non-zero if it contains errors.
 * @
 */
FLECS_API 
int ecs_filter_finalize(
    const ecs_world_t *world,
    ecs_filter_t *filter); 

/** Convert ter, to string expression.
 * Convert term to a string expression. The resulting expression is equivalent
 * to the same term, with the exception of And & Or operators.
 */
FLECS_API
char* ecs_term_str(
    const ecs_world_t *world,
    const ecs_term_t *term);

/** Convert filter to string expression.
 * Convert filter terms to a string expression. The resulting expression can be
 * parsed to create the same filter.
 */
FLECS_API 
char* ecs_filter_str(
    const ecs_world_t *world,
    const ecs_filter_t *filter); 

/** Return a filter iterator.
 * A filter iterator lets an application iterate over entities that match the
 * specified filter. If NULL is provided for the filter, the iterator will
 * iterate all tables in the world.
 * 
 * @param world The world.
 * @param filter The filter.
 * @return An iterator that can be used with ecs_filter_next.
 */
FLECS_API
ecs_iter_t ecs_filter_iter(
    const ecs_world_t *world,
    const ecs_filter_t *filter);  

/** Return a chained filter iterator.
 * A chained iterator applies a filter to the results of the input iterator. The
 * resulting iterator must be iterated with ecs_filter_next.
 *
 * 
 * @param it The input iterator
 * @param filter The filter to apply to the iterator.
 * @return The chained iterator. 
 */
FLECS_API
ecs_iter_t ecs_filter_chain_iter(
    ecs_iter_t *it,
    const ecs_filter_t *filter);

/** Iterate tables matched by filter.
 * This operation progresses the filter iterator to the next table. The 
 * iterator must have been initialized with `ecs_filter_iter`. This operation 
 * must be invoked at least once before interpreting the contents of the 
 * iterator.
 *
 * @param it The iterator
 * @return True if more data is available, false if not.
 */
FLECS_API
bool ecs_filter_next(
    ecs_iter_t *iter);

/** Move resources of one filter to another. */
FLECS_API
void ecs_filter_move(
    ecs_filter_t *dst,
    ecs_filter_t *src);

/** Copy resources of one filter to another. */
FLECS_API
void ecs_filter_copy(
    ecs_filter_t *dst,
    const ecs_filter_t *src);

/** @} */

/**
 * @defgroup queries Queries
 * @{
 */

/** Create a query.
 * This operation creates a query. Queries are used to iterate over entities
 * that match a filter and are the fastest way to find and iterate over entities
 * and their components.
 * 
 * Queries should be created once, and reused multiple times. While iterating a
 * query is a cheap operation, creating and deleting a query is expensive. The
 * reason for this is that queries are "prematched", which means that a query
 * stores state about which entities (or rather, tables) match with the query.
 * Building up this state happens during query creation.
 *
 * Once a query is created, matching only happens when new tables are created.
 * In most applications this is an infrequent process, since it only occurs when
 * a new combination of components is introduced. While matching is expensive,
 * it is importent to note that matching does not happen on a per-entity basis,
 * but on a per-table basis. This means that the average time spent on matching
 * per frame should rapidly approach zero over the lifetime of an application.
 *
 * A query provides direct access to the component arrays. When an application
 * creates/deletes entities or adds/removes components, these arrays can shift
 * component values around, or may grow in size. This can cause unexpected or
 * undefined behavior to occur if these operations are performed while 
 * iterating. To prevent this from happening an application should either not
 * perform these operations while iterating, or use deferred operations (see
 * ecs_defer_begin and ecs_defer_end).
 *
 * Queries can be created and deleted dynamically. If a query was not deleted
 * (using ecs_query_fini) before the world is deleted, it will be deleted 
 * automatically.
 *
 * @param world The world.
 * @param desc A structure describing the query properties.
 * @return The new query.
 */
FLECS_API
ecs_query_t* ecs_query_init(
    ecs_world_t *world, 
    const ecs_query_desc_t *desc);

/** Destroy a query.
 * This operation destroys a query and its resources. If the query is used as
 * the parent of subqueries, those subqueries will be orphaned and must be
 * deinitialized as well.
 *
 * @param query The query.
 */
FLECS_API
void ecs_query_fini(
    ecs_query_t *query);

/** Get filter object of query.
 * This operation obtains a pointer to the internally constructed filter object
 * of the query and can be used to introspect the query terms.
 *
 * @param query The query.
 */
FLECS_API
const ecs_filter_t* ecs_query_get_filter(
    ecs_query_t *query);    

/** Return a query iterator.
 * A query iterator lets an application iterate over entities that match the
 * specified query. If a sorting function is specified, the query will check
 * whether a resort is required upon creating the iterator.
 *
 * Creating a query iterator is a cheap operation that does not allocate any
 * resources. An application does not need to deinitialize or free a query 
 * iterator before it goes out of scope.
 *
 * To iterate the iterator, an application should use ecs_query_next to progress
 * the iterator and test if it has data.
 *
 * Query iteration requires an outer and an inner loop. The outer loop uses
 * ecs_query_next to test if new tables are available. The inner loop iterates
 * the entities in the table, and is usually a for loop that uses iter.count to
 * loop through the entities and component arrays.
 *
 * The two loops are necessary because of how data is stored internally. 
 * Entities are grouped by the components they have, in tables. A single query 
 * can (and often does) match with multiple tables. Because each table has its
 * own set of arrays, an application has to reobtain pointers to those arrays
 * for each matching table.
 *
 * @param world The world or stage, when iterating in readonly mode.
 * @param query The query to iterate.
 * @return The query iterator.
 */
FLECS_API
ecs_iter_t ecs_query_iter(
    ecs_world_t *world,
    ecs_query_t *query);  

/** Iterate over a query.
 * This operation is similar to ecs_query_iter, but starts iterating from a
 * specified offset, and will not iterate more than limit entities.
 *
 * @param world The world or stage, when iterating in readonly mode.
 * @param query The query to iterate.
 * @param offset The number of entities to skip.
 * @param limit The maximum number of entities to iterate.
 * @return The query iterator.
 */
FLECS_API
ecs_iter_t ecs_query_iter_page(
    ecs_world_t *world,
    ecs_query_t *query,
    int32_t offset,
    int32_t limit);  

/** Progress the query iterator.
 * This operation progresses the query iterator to the next table. The 
 * iterator must have been initialized with `ecs_query_iter`. This operation 
 * must be invoked at least once before interpreting the contents of the 
 * iterator.
 *
 * @param iter The iterator.
 * @returns True if more data is available, false if not.
 */
FLECS_API
bool ecs_query_next(
    ecs_iter_t *iter);

/** Progress the query iterator for a worker thread.
 * This operation is similar to ecs_query_next, but provides the ability to 
 * divide entities up across multiple worker threads. The operation accepts a
 * current thread id and a total thread id, which is used to determine which
 * subset of entities should be assigned to the current thread.
 *
 * Current should be less than total, and there should be as many as total
 * threads. If there are less entities in a table than there are threads, only
 * as many threads as there are entities will iterate that table.
 *
 * @param it The iterator.
 * @param stage_current Id of current stage.
 * @param stage_count Total number of stages.
 * @returns True if more data is available, false if not.
 */
FLECS_API
bool ecs_query_next_worker(
    ecs_iter_t *it,
    int32_t stage_current,
    int32_t stage_count);

/** Returns whether the query data changed since the last iteration.
 * This operation must be invoked before obtaining the iterator, as this will
 * reset the changed state. The operation will return true after:
 * - new entities have been matched with
 * - matched entities were deleted
 * - matched components were changed
 * 
 * @param query The query.
 * @return true if entities changed, otherwise false.
 */
FLECS_API
bool ecs_query_changed(
    ecs_query_t *query);

/** Returns whether query is orphaned.
 * When the parent query of a subquery is deleted, it is left in an orphaned
 * state. The only valid operation on an orphaned query is deleting it. Only
 * subqueries can be orphaned.
 *
 * @param query The query.
 * @return true if query is orphaned, otherwise false.
 */
FLECS_API
bool ecs_query_orphaned(
    ecs_query_t *query);

/** @} */


/**
 * @defgroup trigger Triggers
 */

/** Create trigger.
 * Triggers notify the application when certain events happen such as adding or
 * removing components.
 * 
 * An application can change the trigger callback or context pointer by calling
 * ecs_trigger_init for an existing trigger entity, by setting the
 * ecs_trigger_desc_t::entity.entity field in combination with callback and/or
 * ctx.
 *
 * See the documentation for ecs_trigger_desc_t for more details.
 *
 * @param world The world.
 * @param decs The trigger creation parameters.
 */
FLECS_API
ecs_entity_t ecs_trigger_init(
    ecs_world_t *world,
    const ecs_trigger_desc_t *desc);

/** Get trigger context.
 * This operation returns the context pointer set for the trigger. If
 * the provided entity is not a trigger, the function will return NULL.
 *
 * @param world The world.
 * @param trigger The trigger from which to obtain the context.
 * @return The context.
 */
FLECS_API
void* ecs_get_trigger_ctx(
    const ecs_world_t *world,
    ecs_entity_t trigger);

/** Same as ecs_get_trigger_ctx, but for binding ctx. 
 * The binding context is a context typically used to attach any language 
 * binding specific data that is needed when invoking a callback that is 
 * implemented in another language.
 * 
 * @param world The world.
 * @param trigger The trigger from which to obtain the context.
 * @return The context.
 */
FLECS_API
void* ecs_get_trigger_binding_ctx(
    const ecs_world_t *world,
    ecs_entity_t trigger);


typedef enum ecs_payload_kind_t {
    EcsPayloadNone,
    EcsPayloadEntity,
    EcsPayloadTable
} ecs_payload_kind_t;

typedef struct ecs_event_desc_t {
    ecs_entity_t event;
    ecs_ids_t *ids; /* When NULL, notify for all ids in entity/table type */
    ecs_payload_kind_t payload_kind;
    union {
        ecs_entity_t entity;
        struct {
            ecs_table_t *table;
            ecs_table_t *other_table;
            int32_t offset;
            int32_t count; /* When 0 notify all entities starting from offset */
        } table;
    } payload;

    void *param; /* Assigned to iter param member */

    /* Observable for which to notify the triggers/observers. If NULL, the
     * world will be used as observable. */
    ecs_poly_t *observable;
} ecs_event_desc_t;

/** Send event.
 */
FLECS_API
void ecs_emit( 
    ecs_world_t *world,
    ecs_event_desc_t *desc);

/** @} */


/**
 * @defgroup observer Observers
 */

/** Create observer.
 * Observers are like triggers, but can subscribe for multiple terms. An 
 * observer only triggers when the source of the event meets all terms.
 *
 * See the documentation for ecs_observer_desc_t for more details.
 *
 * @param world The world.
 * @param desc The observer creation parameters.
 */
FLECS_API
ecs_entity_t ecs_observer_init(
    ecs_world_t *world,
    const ecs_observer_desc_t *desc);

FLECS_API
void* ecs_get_observer_ctx(
    const ecs_world_t *world,
    ecs_entity_t observer);

FLECS_API
void* ecs_get_observer_binding_ctx(
    const ecs_world_t *world,
    ecs_entity_t observer);

/** @} */


/**
 * @defgroup iterator Iterators
 * @{
 */

/** Obtain data for a query term.
 * This operation retrieves a pointer to an array of data that belongs to the
 * term in the query. The index refers to the location of the term in the query,
 * and starts counting from one.
 *
 * For example, the query "Position, Velocity" will return the Position array
 * for index 1, and the Velocity array for index 2.
 *
 * When the specified term is not owned by the entity this function returns a
 * pointer instead of an array. This happens when the source of a term is not
 * the entity being iterated, such as a shared component (from a prefab), a
 * component from a parent, or another entity. The ecs_term_is_owned operation
 * can be used to test dynamically if a term is owned.
 *
 * The provided size must be either 0 or must match the size of the datatype
 * of the returned array. If the size does not match, the operation may assert.
 * The size can be dynamically obtained with ecs_term_size.
 *
 * @param it The iterator.
 * @param size The size of the returned array.
 * @param index The index of the term in the query.
 * @return A pointer to the data associated with the term.
 */
FLECS_API
void* ecs_term_w_size(
    const ecs_iter_t *it,
    size_t size,
    int32_t index);

/** Test whether the term is readonly
 * This operation returns whether this is a readonly term. Readonly terms are
 * annotated with [in], or are added as a const type in the C++ API.
 *
 * @param it The iterator.
 * @param index The index of the term in the query.
 * @return Whether the term is readonly.
 */
FLECS_API
bool ecs_term_is_readonly(
    const ecs_iter_t *it,
    int32_t index);    

/** Test whether the term is owned
 * This operation returns whether the term is owned by the currently iterated
 * entity. This function will return false when the term is owned by another
 * entity, such as a parent or a prefab.
 *
 * @param it The iterator.
 * @param index The index of the term in the query.
 * @return Whether the term is owned by the iterated over entity/entities.
 */
FLECS_API
bool ecs_term_is_owned(
    const ecs_iter_t *it,
    int32_t index);

/** Convert iterator to string.
 * Prints the contents of an iterator to a string. Useful for debugging and/or
 * testing the output of an iterator.
 * 
 * The function only converts the currently iterated data to a string. To 
 * convert all data, the application has to manually call the next function and
 * call ecs_iter_str on each result.
 * 
 * @param it The iterator.
 * @param next The function used to progress the iterator.
 * @return A string representing the contents of the iterator.
 */
FLECS_API
char* ecs_iter_str(
    const ecs_iter_t *it);

/** Find the column index for a given id.
 * This operation finds the index of a column in the current type for the 
 * specified id. For example, if an entity has type Position, Velocity, and the
 * application requests the id for the Velocity component, this function will
 * return 1.
 *
 * Note that the column index returned by this function starts from 0, as
 * opposed to 1 for the terms. The reason for this is that the returned index
 * is equivalent to using the ecs_type_get_index function.
 *
 * This operation can be used to request columns that are not requested by a
 * query. For example, a query may request Position, Velocity, but an entity
 * may also have Mass. With this function the iterator can request the data for
 * Mass as well, when used in combination with ecs_iter_column.
 *
 * @param it The iterator.
 * @return The type of the currently iterated entity/entities.
 */
FLECS_API
int32_t ecs_iter_find_column(
    const ecs_iter_t *it,
    ecs_id_t id);

/** Obtain data for a column index.
 * This operation can be used with the id obtained from ecs_iter_find_column to
 * request data from the currently iterated over entity/entities that is not
 * requested by the query.
 *
 * The data in the returned pointer can be accessed using the same index as
 * the one used to access the arrays returned by the ecs_term function.
 *
 * The provided size must be either 0 or must match the size of the datatype
 * of the returned array. If the size does not match, the operation may assert.
 * The size can be dynamically obtained with ecs_iter_column_size.
 *
 * Note that this function can be used together with iter::type to 
 * dynamically iterate all data that the matched entities have. An application
 * can use the ecs_vector_count function to obtain the number of elements in a
 * type. All indices from 0..ecs_vector_count(type) are valid column indices.
 *
 * Additionally, note that this provides unprotected access to the column data.
 * An iterator cannot know or prevent accessing columns that are not queried for
 * and thus applications should only use this when it can be guaranteed that
 * there are no other threads reading/writing the same column data.
 *
 * @param it The iterator.
 * @param size The size of the column.
 * @param index The index of the column.
 * @return The data belonging to the column.
 */
FLECS_API
void* ecs_iter_column_w_size(
    const ecs_iter_t *it,
    size_t size,
    int32_t index);

/** Obtain size for a column index.
 * This operation obtains the size for a column. The size is equal to the size
 * of the datatype associated with the column.
 *
 * @param it The iterator.
 * @param index The index of the column.
 * @return The size belonging to the column.
 */
FLECS_API
size_t ecs_iter_column_size(
    const ecs_iter_t *it,
    int32_t index);    

/** @} */


/**
 * @defgroup staging Staging
 * @{
 */

/** Begin frame. 
 * When an application does not use ecs_progress to control the main loop, it
 * can still use Flecs features such as FPS limiting and time measurements. This
 * operation needs to be invoked whenever a new frame is about to get processed.
 *
 * Calls to ecs_frame_begin must always be followed by ecs_frame_end.
 *
 * The function accepts a delta_time parameter, which will get passed to 
 * systems. This value is also used to compute the amount of time the function
 * needs to sleep to ensure it does not exceed the target_fps, when it is set.
 * When 0 is provided for delta_time, the time will be measured.
 *
 * This function should only be ran from the main thread.
 *
 * @param world The world.
 * @param delta_time Time elapsed since the last frame.
 * @return The provided delta_time, or measured time if 0 was provided.
 */
FLECS_API
FLECS_FLOAT ecs_frame_begin(
    ecs_world_t *world,
    FLECS_FLOAT delta_time);

/** End frame. 
 * This operation must be called at the end of the frame, and always after
 * ecs_frame_begin.
 *
 * @param world The world.
 */
FLECS_API
void ecs_frame_end(
    ecs_world_t *world);

/** Begin staging.
 * When an application does not use ecs_progress to control the main loop, it
 * can still use Flecs features such as the defer queue. When an application
 * needs to stage changes, it needs to call this function after ecs_frame_begin.
 * A call to ecs_staging_begin must be followed by a call to ecs_staging_end.
 * 
 * When staging is enabled, modifications to entities are stored to a stage.
 * This ensures that arrays are not modified while iterating. Modifications are
 * merged back to the "main stage" when ecs_staging_end is invoked.
 *
 * While the world is in staging mode, no structural changes (add/remove/...)
 * can be made to the world itself. Operations must be executed on a stage
 * instead (see ecs_get_stage).
 *
 * This function should only be ran from the main thread.
 *
 * @param world The world
 * @return Whether world is currently staged.
 */
FLECS_API
bool ecs_staging_begin(
    ecs_world_t *world);

/** End staging.
 * Leaves staging mode. After this operation the world may be directly mutated
 * again. By default this operation also merges data back into the world, unless
 * automerging was disabled explicitly.
 *
 * This function should only be ran from the main thread.
 *
 * @param world The world
 */
FLECS_API
void ecs_staging_end(
    ecs_world_t *world);

/** Merge world or stage.
 * When automatic merging is disabled, an application can call this
 * operation on either an individual stage, or on the world which will merge
 * all stages. This operation may only be called when staging is not enabled
 * (either after progress() or after staging_end()).
 *
 * This operation may be called on an already merged stage or world.
 *
 * @param world The world.
 */
FLECS_API
void ecs_merge(
    ecs_world_t *world);

/** Defer operations until end of frame. 
 * When this operation is invoked while iterating, operations inbetween the
 * defer_begin and defer_end operations are executed at the end of the frame.
 *
 * This operation is thread safe.
 * 
 * @param world The world.
 * @return true if world changed from non-deferred mode to deferred mode.
 */
FLECS_API
bool ecs_defer_begin(
    ecs_world_t *world);

/** Test if deferring is enabled for current stage.
 * 
 * @param world The world.
 * @return True if deferred, false if not.
 */
FLECS_API
bool ecs_is_deferred(
    const ecs_world_t *world);

/** End block of operations to defer. 
 * See defer_begin.
 *
 * This operation is thread safe.
 *
 * @param world The world.
 * @return true if world changed from deferred mode to non-deferred mode.
 */
FLECS_API
bool ecs_defer_end(
    ecs_world_t *world);

/** Enable/disable automerging for world or stage.
 * When automerging is enabled, staged data will automatically be merged with
 * the world when staging ends. This happens at the end of progress(), at a
 * sync point or when staging_end() is called.
 *
 * Applications can exercise more control over when data from a stage is merged
 * by disabling automerging. This requires an application to explicitly call
 * merge() on the stage.
 *
 * When this function is invoked on the world, it sets all current stages to
 * the provided value and sets the default for new stages. When this function is
 * invoked on a stage, automerging is only set for that specific stage.
 *
 * @param world The world.
 * @param automerge Whether to enable or disable automerging.
 */
FLECS_API
void ecs_set_automerge(
    ecs_world_t *world,
    bool automerge);

/** Configure world to have N stages.
 * This initializes N stages, which allows applications to defer operations to
 * multiple isolated defer queues. This is typically used for applications with
 * multiple threads, where each thread gets its own queue, and commands are
 * merged when threads are synchronized.
 *
 * Note that the ecs_set_threads function already creates the appropriate
 * number of stages. The set_stages() operation is useful for applications that
 * want to manage their own stages and/or threads.
 * 
 * @param world The world.
 * @param stages The number of stages.
 */
FLECS_API
void ecs_set_stages(
    ecs_world_t *world,
    int32_t stages);

/** Get number of configured stages.
 * Return number of stages set by ecs_set_stages.
 *
 * @param world The world.
 * @return The number of stages used for threading.
 */
FLECS_API
int32_t ecs_get_stage_count(
    const ecs_world_t *world);

/** Get current stage id.
 * The stage id can be used by an application to learn about which stage it is
 * using, which typically corresponds with the worker thread id.
 *
 * @param world The world.
 * @return The stage id.
 */
FLECS_API
int32_t ecs_get_stage_id(
    const ecs_world_t *world);

/** Get stage-specific world pointer.
 * Flecs threads can safely invoke the API as long as they have a private 
 * context to write to, also referred to as the stage. This function returns a
 * pointer to a stage, disguised as a world pointer.
 *
 * Note that this function does not(!) create a new world. It simply wraps the
 * existing world in a thread-specific context, which the API knows how to
 * unwrap. The reason the stage is returned as an ecs_world_t is so that it
 * can be passed transparently to the existing API functions, vs. having to 
 * create a dediated API for threading.
 *
 * @param world The world.
 * @param stage_id The index of the stage to retrieve.
 * @return A thread-specific pointer to the world. 
 */
FLECS_API
ecs_world_t* ecs_get_stage(
    const ecs_world_t *world,
    int32_t stage_id);

/** Get actual world from world.
 * @param world A pointer to a stage or the world.
 * @return The world.
 */
FLECS_API
const ecs_world_t* ecs_get_world(
    const ecs_world_t *world);

/** Test whether the current world object is readonly.
 * This function allows the code to test whether the currently used world object
 * is readonly or whether it allows for writing.  
 *
 * @param world A pointer to a stage or the world.
 * @return True if the world or stage is readonly.
 */
FLECS_API
bool ecs_stage_is_readonly(
    const ecs_world_t *stage);

/** Create asynchronous stage.
 * An asynchronous stage can be used to asynchronously queue operations for
 * later merging with the world. An asynchronous stage is similar to a regular
 * stage, except that it does not allow reading from the world.
 *
 * Asynchronous stages are never merged automatically, and must therefore be
 * manually merged with the ecs_merge function. It is not necessary to call
 * defer_begin or defer_end before and after enqueuing commands, as an 
 * asynchronous stage unconditionally defers operations.
 *
 * The application must ensure that no commands are added to the stage while the
 * stage is being merged.
 *
 * An asynchronous stage must be cleaned up by ecs_async_stage_free. 
 *
 * @param world The world.
 * @return The stage.
 */
FLECS_API
ecs_world_t* ecs_async_stage_new(
    ecs_world_t *world);

/** Free asynchronous stage.
 * The provided stage must be an asynchronous stage. If a non-asynchronous stage
 * is provided, the operation will fail.
 *
 * @param stage The stage to free.
 */
FLECS_API
void ecs_async_stage_free(
    ecs_world_t *stage);

/** Test whether provided stage is asynchronous.
 *
 * @param stage The stage.
 * @return True when the stage is asynchronous, false for a regular stage or 
 *         world.
 */
FLECS_API
bool ecs_stage_is_async(
    ecs_world_t *stage);

/** @} */


/**
 * @defgroup table_functions Public table operations
 * @brief Low-level table functions. These functions are intended to enable the
 *        creation of higher-level operations. It is not recommended to use
 *        these operations directly in application code as they do not provide
 *        the same safety guarantees as the other APIs.
 * @{
 */

/** Find or create table with specified component string. 
 * The provided string must be a comma-separated list of fully qualified 
 * component identifiers. The returned table will have the specified components.
 * Two lists that are the same but specify components in a different order will
 * return the same table.
 *
 * @param world The world.
 * @param type The components.
 * @return The new or existing table, or NULL if the string contains an error.
 */
FLECS_API
ecs_table_t* ecs_table_from_str(
    ecs_world_t *world,
    const char *type);

/** Get type for table.
 *
 * @param table The table.
 * @return The type of the table.
 */
FLECS_API
ecs_type_t ecs_table_get_type(
    const ecs_table_t *table);

/** Returns the number of records in the table. 
 * This operation returns the number of records that have been populated through
 * the regular (entity) API as well as the number of records that have been
 * inserted using the direct access API.
 *
 * @param world The world.
 * @param table The table.
 * @return The number of records in a table.
 */
FLECS_API
int32_t ecs_table_count(
    const ecs_table_t *table);

/** Get table that has all components of current table plus the specified id.
 * If the provided table already has the provided id, the operation will return
 * the provided table.
 *
 * @param world The world.
 * @param table The table.
 * @param id The id to add.
 * @result The resulting table.
 */
FLECS_API
ecs_table_t* ecs_table_add_id(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_id_t id);

/** Get table that has all components of current table minus the specified id.
 * If the provided table doesn't have the provided id, the operation will return
 * the provided table.
 *
 * @param world The world.
 * @param table The table.
 * @param id The id to remove.
 * @result The resulting table.
 */
FLECS_API
ecs_table_t* ecs_table_remove_id(
    ecs_world_t *world,
    ecs_table_t *table,
    ecs_id_t id);

/** Lock or unlock table.
 * When a table is locked, modifications to it will trigger an assert. When the 
 * table is locked recursively, it will take an equal amount of unlock
 * operations to actually unlock the table.
 *
 * Table locks can be used to build safe iterators where it is guaranteed that
 * the contents of a table are not modified while it is being iterated.
 *
 * The operation only works when called on the world, and has no side effects 
 * when called on a stage. The assumption is that when called on a stage,
 * operations are deferred already.
 *
 * @param world The world.
 * @param table The table to lock.
 */
FLECS_API
void ecs_table_lock(
    ecs_world_t *world,
    ecs_table_t *table);

/** Unlock a table.
 * Must be called after calling ecs_table_lock.
 *
 * @param world The world.
 * @param table The table to unlock.
 */
FLECS_API
void ecs_table_unlock(
    ecs_world_t *world,
    ecs_table_t *table);    

/** Returns whether table is a module or contains module contents
 * Returns true for tables that have module contents. Can be used to filter out
 * tables that do not contain application data.
 *
 * @param table The table.
 * @return true if table contains module contents, false if not.
 */
FLECS_API
bool ecs_table_has_module(
    ecs_table_t *table);

/** Commit (move) entity to a table.
 * This operation moves an entity from its current table to the specified
 * table. This may trigger the following actions:
 * - Ctor for each component in the target table
 * - Move for each overlapping component
 * - Dtor for each component in the source table.
 * - OnAdd triggers for non-overlapping components in the target table
 * - OnRemove triggers for non-overlapping components in the source table.
 *
 * This operation is a faster than adding/removing components individually.
 *
 * The application must explicitly provide the difference in components between
 * tables as the added/removed parameters. This can usually be derived directly
 * from the result of ecs_table_add_id and esc_table_remove_id. These arrays are
 * required to properly execute OnAdd/OnRemove triggers.
 *
 * @param world The world.
 * @param entity The entity to commit.
 * @param record The entity's record (optional, providing it saves a lookup).
 * @param table The table to commit the entity to.
 * @return True if the entity got moved, false otherwise.
 */
FLECS_API
bool ecs_commit(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_record_t *record,
    ecs_table_t *table,
    ecs_ids_t *added,
    ecs_ids_t *removed);

/** Find record for entity. */
FLECS_API
ecs_record_t* ecs_record_find(
    ecs_world_t *world,
    ecs_entity_t entity);

/** Get component pointer from column/record. */
FLECS_API
void* ecs_record_get_column(
    ecs_record_t *r,
    int32_t column,
    size_t c_size);

/** @} */

/* Include enabled addons */

#ifdef FLECS_SYSTEM
#include "flecs/addons/system.h"
#endif
#ifdef FLECS_PIPELINE
#include "flecs/addons/pipeline.h"
#endif
#ifdef FLECS_TIMER
#include "flecs/addons/timer.h"
#endif
#ifdef FLECS_MODULE
#include "flecs/addons/module.h"
#endif
#ifdef FLECS_PLECS
#include "flecs/addons/plecs.h"
#endif
#ifdef FLECS_PARSER
#include "flecs/addons/parser.h"
#endif
#ifdef FLECS_RULES
#include "flecs/addons/rules.h"
#endif
#ifdef FLECS_SNAPSHOT
#include "flecs/addons/snapshot.h"
#endif
#ifdef FLECS_STATS
#include "flecs/addons/stats.h"
#endif

#include "flecs/addons/flecs_c.h"

#ifdef __cplusplus
}

#ifdef FLECS_CPP
#include "flecs/addons/cpp/flecs.hpp"
#endif

#endif

#endif
