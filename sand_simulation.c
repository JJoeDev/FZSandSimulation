#include <furi.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include <notification/notification.h>
#include <notification/notification_messages.h>

/* generated by fbt from .png files in images folder */
#include <sand_simulation_icons.h>

#define TAG "SandSimulation"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FPS 10

// App menu items
typedef enum{
    SandSimSubmenuIndex_Game,
    SandSimSubmenuIndex_About,
} SandSimSubmenuIndex;

// Different views (screens) to show
typedef enum{
    SandSimView_Submenu, // The menu when the app starts
    SandSimView_Game, // The sand simulation
    SandSimView_About, // The about screen
} SandSimView;

typedef enum{
    SandSimEventID_RedrawScreen = 0,
    SandSimEventID_OkPressed = 42,
} SandSimEventID;

typedef struct{
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications; // Backlight controller
    Submenu* submenu; // App menu
    View* view_game; // The Sand simulation view
    Widget* widget_about;

    FuriTimer* timer;
    
} SandSimApp;

typedef struct{
    FuriMutex* mutex;

    uint8_t x, y;
} SandSimGame;

// Called when Back Button is pressed. Returns VIEW_NONE to indicate application termination
static uint32_t SandSim_navigation_exit_callback(void* ctx){
    UNUSED(ctx);
    return VIEW_NONE;
}

static uint32_t SandSim_navigation_submenu_callback(void* ctx){
    UNUSED(ctx);
    return SandSimView_Submenu;
}

// Called when user selects item from submenu
static void SandSim_submenu_callback(void* ctx, uint32_t index){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_submenu_callback");

    SandSimApp* app = ctx;
    switch(index){
    case SandSimSubmenuIndex_Game:
        view_dispatcher_switch_to_view(app->view_dispatcher, SandSimView_Game);
        break;
    case SandSimSubmenuIndex_About:
        view_dispatcher_switch_to_view(app->view_dispatcher, SandSimView_About);
        break;
    default:
        break;
    }
}

static void SandSim_view_game_draw_callback(Canvas* canvas, void* ssGame){
    furi_assert(ssGame);
    FURI_LOG_T(TAG, "SandSim_view_game_draw_callback");

    SandSimGame* game = ssGame;
    furi_mutex_acquire(game->mutex, FuriWaitForever);

    canvas_draw_dot(canvas, game->x, game->y);

    furi_mutex_release(game->mutex);
}

static void SandSim_view_game_timer_callback(void* ctx){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_view_game_timer_callback");

    SandSimApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, SandSimEventID_RedrawScreen);
}

static void SandSim_view_game_enter_callback(void* ctx){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_view_game_enter_callback");

    SandSimApp* app = ctx;

    furi_assert(app->timer == NULL);
    app->timer = furi_timer_alloc(SandSim_view_game_timer_callback, FuriTimerTypePeriodic, ctx);
    furi_timer_start(app->timer, 1000 / FPS);
}

static void SandSim_view_game_exit_callback(void* ctx){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_view_game_exit_callback");

    SandSimApp* app = ctx;
    furi_timer_stop(app->timer);
    furi_timer_free(app->timer);
    app->timer = NULL;
}

static bool SandSim_view_game_custom_event_callback(uint32_t event, void* ctx){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_view_game_custom_event_callback");

    SandSimApp* app = ctx;
    switch(event){
    case SandSimEventID_RedrawScreen:
        with_view_model(app->view_game, SandSimGame* game, {UNUSED(game);}, true);
        return true;
        break;
    case SandSimEventID_OkPressed:
        return true;
        break;
    default:
        return false;
        break;
    }
}

static bool SandSim_view_game_input_callback(InputEvent* i_event, void* ctx){
    furi_assert(ctx);
    FURI_LOG_T(TAG, "SandSim_view_game_input_callback");

    SandSimApp* app = ctx;
    if(i_event->type == InputTypeShort){
        if(i_event->key == InputKeyLeft){
            with_view_model(app->view_game, SandSimGame* game, {if(game->x > 1) game->x--;}, true);
        }
        else if(i_event->key == InputKeyRight){
            with_view_model(app->view_game, SandSimGame* game, {if(game->x < SCREEN_WIDTH) game->x++;}, true);
        }
    }
    else if(i_event->type == InputTypePress){
        if(i_event->key == InputKeyOk){
            view_dispatcher_send_custom_event(app->view_dispatcher, SandSimEventID_OkPressed);
            return true;
        }
    }

    return false;
}

static SandSimApp* SandSim_application_alloc(){
    SandSimApp* app = malloc(sizeof(SandSimApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(
        app->submenu, 
        "Play", 
        SandSimSubmenuIndex_Game, 
        SandSim_submenu_callback, 
        app);
    submenu_add_item(
        app->submenu, 
        "About", 
        SandSimSubmenuIndex_About, 
        SandSim_submenu_callback, 
        app);
    
    view_set_previous_callback(submenu_get_view(app->submenu), SandSim_navigation_exit_callback);

    view_dispatcher_add_view(app->view_dispatcher, SandSimView_Submenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, SandSimView_Submenu);

    app->view_game = view_alloc();
    view_set_draw_callback(app->view_game, SandSim_view_game_draw_callback);
    view_set_input_callback(app->view_game, SandSim_view_game_input_callback);
    view_set_previous_callback(app->view_game, SandSim_navigation_submenu_callback);
    view_set_enter_callback(app->view_game, SandSim_view_game_enter_callback);
    view_set_exit_callback(app->view_game, SandSim_view_game_exit_callback);
    view_set_context(app->view_game, app);
    view_set_custom_callback(app->view_game, SandSim_view_game_custom_event_callback);
    view_allocate_model(app->view_game, ViewModelTypeLockFree, sizeof(SandSimGame));

    SandSimGame* game = view_get_model(app->view_game);
    game->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    game->x = SCREEN_WIDTH / 2;
    game->y = SCREEN_HEIGHT / 2;
    view_dispatcher_add_view(app->view_dispatcher, SandSimView_Game, app->view_game);

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about, 
        0, 
        0, 
        SCREEN_WIDTH, 
        SCREEN_HEIGHT, 
        "This is a simple\nSand Simulation.\n\nThis app is supposed to be\nsimple and satisfying to use\nand help me learn how to\nbuild for the Flipper Zero"); // About 31 char per line
    
    view_set_previous_callback(widget_get_view(app->widget_about), SandSim_navigation_submenu_callback);
    view_dispatcher_add_view(app->view_dispatcher, SandSimView_About, widget_get_view(app->widget_about));

    return app;
}

static void SandSim_application_free(SandSimApp* app){
    view_dispatcher_remove_view(app->view_dispatcher, SandSimView_About);
    widget_free(app->widget_about);

    view_dispatcher_remove_view(app->view_dispatcher, SandSimView_Game);
    view_free(app->view_game);

    view_dispatcher_remove_view(app->view_dispatcher, SandSimView_Submenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t sand_simulation_app(void* p){
    UNUSED(p);

    SandSimApp* app = SandSim_application_alloc();
    view_dispatcher_run(app->view_dispatcher);

    SandSim_application_free(app);

    return 0;
}