#include "includes.h"
#include "discord-config.h"
#include "options.h"
#include "interface.h"

#include "utils/lib-utils/ida-utils.h"
#include "utils/lib-utils/discord-utils.h"

#ifdef _Release
#include "utils/build-version/ver.h"
#endif
#ifdef _Release64
#include "utils/build-version/ver64.h"
#endif

const char* IDAP_comment = "IDA RPC 9.3 by Karami";
const char* IDAP_help    = "IDA RPC 9.3 by Karami";
const char* IDAP_name    = "IDA RPC 9.3";
const char* IDAP_hotkey  = "Ctrl-Alt-R";

class ida_rpc_plugin_t;

class rpc_event_listener_t : public event_listener_t
{
public:
    rpc_event_listener_t( ida_rpc_plugin_t* owner, hook_type_t hook_type )
        : owner( owner ), hook_type( hook_type ) { }

    virtual ssize_t idaapi on_event( ssize_t notification_code, va_list va ) override;

private:
    ida_rpc_plugin_t* owner;
    hook_type_t hook_type;
};

class ida_rpc_plugin_t : public plugmod_t
{
public:
    ida_rpc_plugin_t( )
        : idp_listener( this, HT_IDP ),
          idb_listener( this, HT_IDB ),
          ui_listener( this, HT_UI ),
          view_listener( this, HT_VIEW ) { }

    virtual ~ida_rpc_plugin_t( ) override
    {
        unhook_callbacks( );

        if ( discord_initialized ) {
            Discord_ClearPresence( );
            Discord_Shutdown( );
        }
    }

    bool init( )
    {
        g_options.load( );  // loaded before callbacks so they are able to output

        start_time = time( 0 );

        initialize_discord( );

        if ( g_options.rpc_enabled && !hook_callbacks( ) ) {
            if ( g_options.output_type >= ( int ) output_type::errors_results_and_interim_steps && g_options.output_enabled ) {
                msg( "[%s] %s -> one or more callback hooks failed plugin loading will be skipped\n", IDAP_name, __FUNCTION__ );
            }

            return false;
        }

        update_presence( );
        return true;
    }

    virtual bool idaapi run( size_t arg ) override
    {
        show_options( );

        if ( g_options.rpc_enabled ) {
            hook_callbacks( );

            if ( ida_utils::is_idb_loaded( ) && g_options.output_type >= ( int ) output_type::errors_and_results && g_options.output_enabled ) {
                msg( "[%s] Version: %.2f\n", IDAP_name, AUTO_VERSION_RELEASE );
                msg( "[%s] Built for IDA SDK: %i\n", IDAP_name, IDA_SDK_VERSION );
                msg( "[%s] IDP interface: %i\n", IDAP_name, IDP_INTERFACE_VERSION );
                msg( "[%s] Processor module: %s \n", IDAP_name, ida_utils::get_current_processor_module( ) );
                msg( "[%s] Currently open file: %s\n", IDAP_name, ida_utils::get_current_filename( ) );
                msg( "[%s] Current function: %s - 0x%a\n", IDAP_name, ida_utils::get_current_function_name( ), ida_utils::get_current_function_start_address( ) );
                msg( "[%s] Current selected address: 0x%a\n", IDAP_name, ida_utils::get_current_cursor_address( ) );
            }

            update_presence( );
        } else {
            unhook_callbacks( );
            Discord_ClearPresence( );
        }

        return true;
    }

    ssize_t handle_event( hook_type_t hook_type, ssize_t notification_code, va_list /*va*/ )
    {
        bool should_update = false;

        switch ( hook_type )
        {
        case HT_IDP:
            switch ( notification_code )
            {
            case processor_t::ev_oldfile:
            case processor_t::ev_newfile:
            case processor_t::ev_newbinary:
            case processor_t::ev_rename:
                should_update = true;
                break;
            }
            break;

        case HT_IDB:
            switch ( notification_code )
            {
            case idb_event::savebase:
            case idb_event::func_updated:
            case idb_event::set_func_start:
            case idb_event::renamed:
            case idb_event::func_added:
            case idb_event::deleting_func:
            case idb_event::allsegs_moved:
                should_update = true;
                break;
            }
            break;

        case HT_UI:
            switch ( notification_code )
            {
            case ui_load_file:
            case ui_updated_actions:
            case ui_refresh:
            case ui_get_cursor:
            case ui_get_curline:
                should_update = true;
                break;
            }
            break;

        case HT_VIEW:
            switch ( notification_code )
            {
            case view_loc_changed:
            case view_switched:
            case view_click:
            case view_curpos:
                should_update = true;
                break;
            }
            break;
        }

        if ( should_update ) {
            update_presence( );
        }

        return 0;
    }

private:
    bool hook_one( hook_type_t hook_type, event_listener_t* listener, bool& is_hooked, const char* name )
    {
        if ( is_hooked ) {
            return true;
        }

        is_hooked = hook_event_listener( hook_type, listener, HKCB_GLOBAL );

        if ( g_options.output_type >= ( int ) output_type::errors_results_and_interim_steps && g_options.output_enabled ) {
            msg( "[%s] hook_event_listener( %s ) %s\n", IDAP_name, name, is_hooked ? "was successful" : "failed" );
        } else if ( !is_hooked && g_options.output_type >= ( int ) output_type::errors_only && g_options.output_enabled ) {
            msg( "[%s] hook_event_listener( %s ) failed\n", IDAP_name, name );
        }

        return is_hooked;
    }

    void unhook_one( hook_type_t hook_type, event_listener_t* listener, bool& is_hooked )
    {
        if ( is_hooked ) {
            unhook_event_listener( hook_type, listener );
            is_hooked = false;
        }
    }

    bool hook_callbacks( )
    {
        if ( idp_hooked && idb_hooked && ui_hooked && view_hooked ) {
            return true;
        }

        bool hooks_ok = hook_one( HT_IDP, &idp_listener, idp_hooked, "HT_IDP" );
        hooks_ok = hook_one( HT_IDB, &idb_listener, idb_hooked, "HT_IDB" ) && hooks_ok;
        hooks_ok = hook_one( HT_UI, &ui_listener, ui_hooked, "HT_UI" ) && hooks_ok;
        hooks_ok = hook_one( HT_VIEW, &view_listener, view_hooked, "HT_VIEW" ) && hooks_ok;

        if ( !hooks_ok ) {
            unhook_callbacks( );
        }

        return hooks_ok;
    }

    void unhook_callbacks( )
    {
        unhook_one( HT_IDP, &idp_listener, idp_hooked );
        unhook_one( HT_IDB, &idb_listener, idb_hooked );
        unhook_one( HT_UI, &ui_listener, ui_hooked );
        unhook_one( HT_VIEW, &view_listener, view_hooked );
    }

    void update_presence( )
    {
        if ( g_options.rpc_enabled ) {
            discord_utils::update_discord_presence( start_time );
        }
    }

    void initialize_discord( )
    {
        if ( discord_initialized ) {
            Discord_ClearPresence( );
            Discord_Shutdown( );
            discord_initialized = false;
        }

        discord_utils::discord_init( discord_config::application_id );
        discord_initialized = true;
    }

private:
    int64_t start_time = 0;
    bool discord_initialized = false;

    rpc_event_listener_t idp_listener;
    rpc_event_listener_t idb_listener;
    rpc_event_listener_t ui_listener;
    rpc_event_listener_t view_listener;

    bool idp_hooked = false;
    bool idb_hooked = false;
    bool ui_hooked = false;
    bool view_hooked = false;
};

ssize_t idaapi rpc_event_listener_t::on_event( ssize_t notification_code, va_list va )
{
    return owner->handle_event( hook_type, notification_code, va );
}

static plugmod_t* idaapi IDAP_init( void )
{
    addon_info_t addon = { };
    addon.name     = "IDA RPC 9.3";

    register_addon( &addon );

    ida_rpc_plugin_t* plugin = new ida_rpc_plugin_t( );
    if ( plugin->init( ) ) {
        return plugin;
    }

    delete plugin;
    return PLUGIN_SKIP;
}

plugin_t PLUGIN = {
    IDP_INTERFACE_VERSION,  // IDA version plugin was written for
    PLUGIN_FIX | PLUGIN_MULTI, // Plugin flags
    IDAP_init,              // Init func
    nullptr,                // Clean up func
    nullptr,                // Main plugin body
    IDAP_comment,           // Comment for status line or for hints
    IDAP_help,              // Multiline help for the plugin
    IDAP_name,              // Plugin name shown in edit -> plugins menu
    IDAP_hotkey             // Hotkey to run the plugin
};
