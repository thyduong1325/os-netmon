#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <unistd.h>
#include <iomanip>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// Application window dimensions
#define WIDTH 820
#define HEIGHT 600

// Defines the three possible sorting columns
enum SortMode { SORT_NAME, SORT_RX, SORT_TX };

// Holds the raw data parsed from the OS for a single network interface
typedef struct NetEntry {
    std::string interface_name;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} NetEntry;

// Holds the SDL textures and positioning data for rendering a single row
typedef struct GNetEntry {
    SDL_Texture *icon_tex; // Pointer to the shared icon texture
    SDL_Rect icon_pos;     // Bounding box for the icon
    SDL_Texture *name_tex;
    SDL_Texture *rx_tex;
    SDL_Texture *tx_tex;
    SDL_Rect name_pos;
    SDL_Rect rx_pos;
    SDL_Rect tx_pos;
} GNetEntry;

// Central data structure holding the application state and shared assets
typedef struct AppData {
    TTF_Font *font;
    TTF_Font *header_font;
    std::vector<NetEntry*> entries;             // Raw parsed data
    std::vector<GNetEntry*> graphic_entries;    // Renderable textures
    
    // Header Textures & Positions
    SDL_Texture *header_name;
    SDL_Texture *header_rx;
    SDL_Texture *header_tx;
    SDL_Rect header_name_pos;
    SDL_Rect header_rx_pos;
    SDL_Rect header_tx_pos;

    // Shared Image Textures
    SDL_Texture *logo_tex;
    SDL_Texture *icon_eth;
    SDL_Texture *icon_lo;
    SDL_Texture *icon_virt;
    SDL_Texture *icon_wifi;
    SDL_Rect logo_pos;
    
    // UI State Variables
    int scrollY;
    SortMode current_sort;
    bool sort_ascending; // Tracks the direction of the sort (true = UP, false = DOWN)
} AppData;

// Function Prototypes
void initialize(SDL_Renderer *renderer, AppData *data_ptr);
void updateHeaders(SDL_Renderer *renderer, AppData *data_ptr); 
void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr);
void render(SDL_Renderer *renderer, AppData *data_ptr);
void parseNetworkData(std::vector<NetEntry*> &entries);
void populateGraphics(SDL_Renderer *renderer, AppData *data_ptr);
void clearGFiles(std::vector<GNetEntry*>& graphic_entries);
bool pointInRect(int x, int y, SDL_Rect& rect);
void quit(AppData *data_ptr);


// MAIN ENTRY POINT
// Initializes systems, sets up the application state, and runs the 
// non-blocking event and render loop.

int main(int argc, char *argv[])
{
    // Initialize core SDL subsystems, PNG image loading, and TrueType fonts
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    // Create the main application window and its associated 2D hardware renderer
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "OS Network Traffic Monitor");

    // Initialize application state variables
    AppData data;
    data.scrollY = 0;
    data.current_sort = SORT_NAME;
    data.sort_ascending = true;
    
    // Null pointers before texture creation to prevent garbage memory issues
    data.header_name = NULL;
    data.header_rx = NULL;
    data.header_tx = NULL;

    // Load assets and prepare the initial data
    initialize(renderer, &data);

    // Main Game Loop Variables
    SDL_Event event;
    bool isRunning = true;
    Uint32 lastUpdateTime = 0;

    // Non-blocking application loop
    while (isRunning)
    {
        // 1. Process all pending user input events
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) isRunning = false;
            handleEvent(&event, renderer, &data);
        }

        // 2. Periodically update the network data from the OS
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastUpdateTime > 500) // 500ms interval
        {
            populateGraphics(renderer, &data);
            lastUpdateTime = currentTime;
        }

        // 3. Render the current frame
        render(renderer, &data);
        
        // 4. Cap framerate at roughly 60 FPS to prevent burning CPU cycles
        SDL_Delay(16); 
    }

    // Free memory and shut down SDL subsystems
    quit(&data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}


// INITIALIZE
// Loads all external assets (fonts, images) and creates the initial
// UI state (headers and first batch of network graphics).

void initialize(SDL_Renderer *renderer, AppData *data_ptr)
{
    // Load local font for rows and system font for headers (for UTF-8 arrow support)
    data_ptr->font = TTF_OpenFont("resrc/fonts/OpenSans-Regular.ttf", 20);
    data_ptr->header_font = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24);

    // Load university logo image
    SDL_Surface *surf = IMG_Load("resrc/images/UST-logo.png");
    data_ptr->logo_tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    data_ptr->logo_pos = {WIDTH - 140, 5, 130, 40};

    // Load network interface icons
    surf = IMG_Load("resrc/images/ethernet.png");
    data_ptr->icon_eth = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    surf = IMG_Load("resrc/images/loopback.png");
    data_ptr->icon_lo = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    surf = IMG_Load("resrc/images/virtual.png");
    data_ptr->icon_virt = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    surf = IMG_Load("resrc/images/wifi.png");
    data_ptr->icon_wifi = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    // Generate the initial static UI elements
    updateHeaders(renderer, data_ptr);
    populateGraphics(renderer, data_ptr);
}


// UPDATE HEADERS
// Dynamically generates the header text textures, appending UTF-8
// UP or DOWN arrows based on the current sorting state.

void updateHeaders(SDL_Renderer *renderer, AppData *data_ptr) 
{
    // Destroy old textures if they exist to prevent memory leaks during updates
    if (data_ptr->header_name) SDL_DestroyTexture(data_ptr->header_name);
    if (data_ptr->header_rx) SDL_DestroyTexture(data_ptr->header_rx);
    if (data_ptr->header_tx) SDL_DestroyTexture(data_ptr->header_tx);

    // Base header strings
    std::string name_str = "Interface";
    std::string rx_str = "Download";
    std::string tx_str = "Upload";

    // Determine correct UTF-8 arrow based on sort direction
    // \xE2\x86\x91 = Up Arrow (U+2191), \xE2\x86\x93 = Down Arrow (U+2193)
    std::string arrow = data_ptr->sort_ascending ? " \xE2\x86\x91" : " \xE2\x86\x93";

    // Append the arrow only to the currently active sorting column
    if (data_ptr->current_sort == SORT_NAME) name_str += arrow;
    else if (data_ptr->current_sort == SORT_RX) rx_str += arrow;
    else if (data_ptr->current_sort == SORT_TX) tx_str += arrow;

    // Theme color: St. Thomas Purple
    SDL_Color header_color = { 81, 12, 118, 255 };

    // Render "Interface" header
    SDL_Surface *surf = TTF_RenderUTF8_Solid(data_ptr->header_font, name_str.c_str(), header_color);
    data_ptr->header_name = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_QueryTexture(data_ptr->header_name, NULL, NULL, &(data_ptr->header_name_pos.w), &(data_ptr->header_name_pos.h));
    data_ptr->header_name_pos.x = 45; data_ptr->header_name_pos.y = 10;
    SDL_FreeSurface(surf);

    // Render "Download" header
    surf = TTF_RenderUTF8_Solid(data_ptr->header_font, rx_str.c_str(), header_color);
    data_ptr->header_rx = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_QueryTexture(data_ptr->header_rx, NULL, NULL, &(data_ptr->header_rx_pos.w), &(data_ptr->header_rx_pos.h));
    data_ptr->header_rx_pos.x = 300; data_ptr->header_rx_pos.y = 10;
    SDL_FreeSurface(surf);

    // Render "Upload" header
    surf = TTF_RenderUTF8_Solid(data_ptr->header_font, tx_str.c_str(), header_color);
    data_ptr->header_tx = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_QueryTexture(data_ptr->header_tx, NULL, NULL, &(data_ptr->header_tx_pos.w), &(data_ptr->header_tx_pos.h));
    data_ptr->header_tx_pos.x = 550; data_ptr->header_tx_pos.y = 10;
    SDL_FreeSurface(surf);
}


// HANDLE EVENT
// Processes user inputs, specifically mouse clicks for sorting 
// and the mouse wheel for scrolling.

void handleEvent(SDL_Event *event, SDL_Renderer *renderer, AppData *data_ptr)
{
    // Handle Left Mouse Clicks
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT)
    {
        int x = event->button.x;
        int y = event->button.y;

        // Check if the user clicked the "Interface" header
        if (pointInRect(x, y, data_ptr->header_name_pos)) 
        {
            if (data_ptr->current_sort == SORT_NAME) 
            {
                data_ptr->sort_ascending = !data_ptr->sort_ascending; // Toggle direction if already active
            } 
            else 
            {
                data_ptr->current_sort = SORT_NAME; // Switch focus to this column
                data_ptr->sort_ascending = true;    // Reset to ascending
            }
            updateHeaders(renderer, data_ptr);      // Redraw arrows
            populateGraphics(renderer, data_ptr);   // Re-sort data
        }
        // Check if the user clicked the "Download" header
        else if (pointInRect(x, y, data_ptr->header_rx_pos)) 
        {
            if (data_ptr->current_sort == SORT_RX) 
            {
                data_ptr->sort_ascending = !data_ptr->sort_ascending;
            }
            else 
            {
                data_ptr->current_sort = SORT_RX;
                data_ptr->sort_ascending = true;
            }
            updateHeaders(renderer, data_ptr);
            populateGraphics(renderer, data_ptr);
        }
        // Check if the user clicked the "Upload" header
        else if (pointInRect(x, y, data_ptr->header_tx_pos)) 
        {
            if (data_ptr->current_sort == SORT_TX) 
            {
                data_ptr->sort_ascending = !data_ptr->sort_ascending;
            } 
            else 
            {
                data_ptr->current_sort = SORT_TX;
                data_ptr->sort_ascending = true;
            }
            updateHeaders(renderer, data_ptr);
            populateGraphics(renderer, data_ptr);
        }
    }
    // Handle Mouse Wheel Scrolling
    else if (event->type == SDL_MOUSEWHEEL)
    {
        data_ptr->scrollY += 20 * event->wheel.y; // Apply scroll speed multiplier
        
        // Prevent scrolling above the very top item
        if (data_ptr->scrollY > 0) 
        {
            data_ptr->scrollY = 0;
        }
        
        // Calculate the maximum possible scroll distance
        int total_content_height = 70 + (40 * data_ptr->graphic_entries.size());
        int viewport_height = HEIGHT - 50;

        // Artificial padding to ensure the scrollbar is always active for demonstration
        if (total_content_height <= viewport_height) 
        {
            total_content_height = viewport_height + 200; 
        }
        
        int max_scroll = viewport_height - total_content_height;
        
        // Prevent scrolling below the last item
        if (data_ptr->scrollY < max_scroll)
        {
            data_ptr->scrollY = max_scroll;
        }
    }
}


// RENDER
// Draws all elements to the screen: backgrounds, data rows, UI headers,
// the university logo, and the custom visual scrollbar.

void render(SDL_Renderer *renderer, AppData *data_ptr)
{
    // Clear screen to a light gray background
    SDL_SetRenderDrawColor(renderer, 240, 245, 250, 255); 
    SDL_RenderClear(renderer);

    // 1. Draw Data Rows (Applying Scroll Offset)
    for (int i = 0; i < data_ptr->graphic_entries.size(); i++)
    {
        GNetEntry *g = data_ptr->graphic_entries[i];
        
        // Offset Y positions by current scroll amount
        SDL_Rect icon_dst = g->icon_pos; icon_dst.y += data_ptr->scrollY;
        SDL_Rect name_dst = g->name_pos; name_dst.y += data_ptr->scrollY;
        SDL_Rect rx_dst = g->rx_pos;     rx_dst.y += data_ptr->scrollY;
        SDL_Rect tx_dst = g->tx_pos;     tx_dst.y += data_ptr->scrollY;

        // Clip rendering: only draw items that sit below the 40px header boundary
        if (name_dst.y > 40) 
        {
            SDL_RenderCopy(renderer, g->icon_tex, NULL, &icon_dst);
            SDL_RenderCopy(renderer, g->name_tex, NULL, &name_dst);
            SDL_RenderCopy(renderer, g->rx_tex, NULL, &rx_dst);
            SDL_RenderCopy(renderer, g->tx_tex, NULL, &tx_dst);
        }
    }

    // 2. Draw Header Background & Divider Line (Always on top of data)
    SDL_Rect header_bg = {0, 0, WIDTH, 50};
    SDL_SetRenderDrawColor(renderer, 245, 240, 250, 255);
    SDL_RenderFillRect(renderer, &header_bg);
    SDL_SetRenderDrawColor(renderer, 81, 12, 118, 255); // St. Thomas Purple Divider
    SDL_RenderDrawLine(renderer, 0, 50, WIDTH, 50);

    // 3. Draw Header Text
    SDL_RenderCopy(renderer, data_ptr->header_name, NULL, &(data_ptr->header_name_pos));
    SDL_RenderCopy(renderer, data_ptr->header_rx, NULL, &(data_ptr->header_rx_pos));
    SDL_RenderCopy(renderer, data_ptr->header_tx, NULL, &(data_ptr->header_tx_pos));

    // 4. Draw Logo
    SDL_RenderCopy(renderer, data_ptr->logo_tex, NULL, &(data_ptr->logo_pos));
    
    // 5. Draw Custom Visual Scrollbar
    int total_content_height = 70 + (40 * data_ptr->graphic_entries.size());
    int viewport_height = HEIGHT - 50; 

    // Artificial padding logic to match scrolling bounds
    if (total_content_height <= viewport_height) {
        total_content_height = viewport_height + 200; 
    }

    // Draw the static scroll track
    SDL_Rect scroll_track = { WIDTH - 15, 50, 15, viewport_height };
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &scroll_track);

    // Calculate dynamic size of the scroll thumb
    float visible_ratio = (float)viewport_height / (float)total_content_height;
    int thumb_height = (int)(viewport_height * visible_ratio);
    
    if (thumb_height < 30) thumb_height = 30; // Enforce minimum thumb size

    // Calculate dynamic Y position of the scroll thumb
    int max_scroll_distance = total_content_height - viewport_height;
    float scroll_percentage = (float)(-data_ptr->scrollY) / (float)max_scroll_distance;
    
    int available_track_space = viewport_height - thumb_height;
    int thumb_y = 50 + (int)(scroll_percentage * available_track_space);

    // Draw the moving scroll thumb
    SDL_Rect scroll_thumb = { WIDTH - 15, thumb_y, 15, thumb_height };
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255); 
    SDL_RenderFillRect(renderer, &scroll_thumb);

    // 6. Push buffer to screen
    SDL_RenderPresent(renderer);
}


// PARSE NETWORK DATA
// Opens the Linux virtual filesystem (/proc/net/dev) and extracts
// the raw upload and download bytes for every network interface.

void parseNetworkData(std::vector<NetEntry*> &entries)
{
    // Clear old data to prevent memory leaks during polling
    for (auto entry : entries) 
    { 
        delete entry; 
    }
    entries.clear();

    // Open kernel network stats file
    std::ifstream file("/proc/net/dev");
    std::string line;

    // Skip the first two header lines inside the file
    std::getline(file, line);
    std::getline(file, line);

    // Parse each remaining line
    while (std::getline(file, line)) 
    {
        std::istringstream iss(line);
        std::string interface_name;
        iss >> interface_name; // Extracts first string (e.g., "eth0:")

        // Remove the trailing colon from the interface name
        if (!interface_name.empty() && interface_name.back() == ':')
        {
            interface_name.pop_back();
        }

        // Setup variables for the 9 columns of data we must parse through
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes;

        // Extract numbers. We only care about the 1st (rx_bytes) and 9th (tx_bytes)
        if (iss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes) 
        {
            // Allocate and store new entry
            NetEntry* entry = new NetEntry();
            entry->interface_name = interface_name;
            entry->rx_bytes = rx_bytes;
            entry->tx_bytes = tx_bytes;
            entries.push_back(entry);
        }
    }
}


// FORMAT BYTES
// Converts raw byte counts into human-readable strings (KB, MB, GB).

std::string formatBytes(uint64_t bytes) 
{
    const char* suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int suffixIndex = 0;
    double count = static_cast<double>(bytes);

    // Keep dividing by 1024 until the number is small enough, 
    // or we hit the maximum unit (TB)
    while (count >= 1024.0 && suffixIndex < 4) 
    {
        count /= 1024.0;
        suffixIndex++;
    }

    std::stringstream ss;
    if (suffixIndex == 0) 
    {
        // If it's just Bytes, don't show decimals
        ss << static_cast<uint64_t>(count) << " " << suffixes[suffixIndex];
    } 
    else 
    {
        // For KB, MB, etc., show exactly 2 decimal places
        ss << std::fixed << std::setprecision(2) << count << " " << suffixes[suffixIndex];
    }
    
    return ss.str();
}


// POPULATE GRAPHICS
// Converts the raw network data into sortable SDL Textures, assigning
// icons based on interface naming conventions.

void populateGraphics(SDL_Renderer *renderer, AppData *data_ptr)
{
    // Safely clear out the previous frame's textures
    clearGFiles(data_ptr->graphic_entries);
    
    // Fetch fresh data
    parseNetworkData(data_ptr->entries);

    // Sort Data using C++ lambdas based on current UI state
    if (data_ptr->current_sort == SORT_NAME) 
    {
        if (data_ptr->sort_ascending)
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->interface_name < b->interface_name; });
        }
        else
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->interface_name > b->interface_name; });
        }
    } 
    else if (data_ptr->current_sort == SORT_RX) 
    {
        if (data_ptr->sort_ascending)
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->rx_bytes < b->rx_bytes; });
        }
        else
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->rx_bytes > b->rx_bytes; });
        }
    } 
    else if (data_ptr->current_sort == SORT_TX) 
    {
        if (data_ptr->sort_ascending)
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->tx_bytes < b->tx_bytes; });
        }
        else
        {
            std::sort(data_ptr->entries.begin(), data_ptr->entries.end(), [](const NetEntry* a, const NetEntry* b) { return a->tx_bytes > b->tx_bytes; });
        }
    }

    SDL_Color color = { 30, 30, 30, 255 }; // Dark text for data rows
    char buffer[64];

    // Generate graphics for each sorted entry
    for (int i = 0; i < data_ptr->entries.size(); i++)
    {
        GNetEntry *g_entry = new GNetEntry();
        std::string name = data_ptr->entries[i]->interface_name;

        // Assign Icon based on Linux Interface Naming Conventions
        if (name.substr(0, 2) == "en" || name.substr(0, 3) == "eth") 
        {
            g_entry->icon_tex = data_ptr->icon_eth;
        } 
        else if (name.substr(0, 2) == "wl" || name.substr(0, 4) == "wlan") 
        {
            g_entry->icon_tex = data_ptr->icon_wifi;
        } 
        else if (name == "lo") 
        {
            g_entry->icon_tex = data_ptr->icon_lo;
        } 
        else 
        {
            g_entry->icon_tex = data_ptr->icon_virt; // Fallback to virtual icon
        }

        // Set fixed Icon Position (24x24 pixels)
        g_entry->icon_pos.w = 24;
        g_entry->icon_pos.h = 24;
        g_entry->icon_pos.x = 10;
        g_entry->icon_pos.y = 70 + (40 * i);

        // Render Interface Name Text
        SDL_Surface *name_surf = TTF_RenderText_Solid(data_ptr->font, name.c_str(), color);
        g_entry->name_tex = SDL_CreateTextureFromSurface(renderer, name_surf);
        SDL_FreeSurface(name_surf);
        SDL_QueryTexture(g_entry->name_tex, NULL, NULL, &(g_entry->name_pos.w), &(g_entry->name_pos.h));
        
        // Push text to x=45 to leave room for the icon
        g_entry->name_pos.x = 45;
        g_entry->name_pos.y = 70 + (40 * i);

        // Render Download Text (Formatted)
        std::string rx_formatted = formatBytes(data_ptr->entries[i]->rx_bytes);
        SDL_Surface *rx_surf = TTF_RenderText_Solid(data_ptr->font, rx_formatted.c_str(), color);
        g_entry->rx_tex = SDL_CreateTextureFromSurface(renderer, rx_surf);
        SDL_FreeSurface(rx_surf);
        SDL_QueryTexture(g_entry->rx_tex, NULL, NULL, &(g_entry->rx_pos.w), &(g_entry->rx_pos.h));
        g_entry->rx_pos.x = 300;
        g_entry->rx_pos.y = 70 + (40 * i);

        // Render Upload Text (Formatted)
        std::string tx_formatted = formatBytes(data_ptr->entries[i]->tx_bytes);
        SDL_Surface *tx_surf = TTF_RenderText_Solid(data_ptr->font, tx_formatted.c_str(), color);
        g_entry->tx_tex = SDL_CreateTextureFromSurface(renderer, tx_surf);
        SDL_FreeSurface(tx_surf);
        SDL_QueryTexture(g_entry->tx_tex, NULL, NULL, &(g_entry->tx_pos.w), &(g_entry->tx_pos.h));
        g_entry->tx_pos.x = 550;
        g_entry->tx_pos.y = 70 + (40 * i);

        // Store the fully prepared row
        data_ptr->graphic_entries.push_back(g_entry);
    }
}


// CLEAR GRAPHIC FILES
// Safely cleans up the textures generated every 500ms to prevent 
// massive memory leaks.

void clearGFiles(std::vector<GNetEntry*>& graphic_entries)
{
    for (int i = 0; i < graphic_entries.size(); i++)
    {
        // Note: We DO NOT destroy icon_tex here, because it points 
        // to a shared texture loaded once during initialization.
        SDL_DestroyTexture(graphic_entries[i]->name_tex);
        SDL_DestroyTexture(graphic_entries[i]->rx_tex);
        SDL_DestroyTexture(graphic_entries[i]->tx_tex);
        delete graphic_entries[i];
    }
    graphic_entries.clear();
}


// POINT IN RECT
// Simple collision detection math to check if a mouse click (x, y)
// falls inside a specific SDL_Rect bounds.

bool pointInRect(int x, int y, SDL_Rect& rect)
{
    return (x > rect.x && x < rect.x + rect.w && y > rect.y && y < rect.y + rect.h);
}


// QUIT
// Final cleanup function called before application exit. Destroys
// all remaining shared assets and frees memory.

void quit(AppData *data_ptr)
{
    // Clean up current rows
    clearGFiles(data_ptr->graphic_entries);
    for (auto entry : data_ptr->entries) { delete entry; }
    
    // Clean up UI headers
    SDL_DestroyTexture(data_ptr->header_name);
    SDL_DestroyTexture(data_ptr->header_rx);
    SDL_DestroyTexture(data_ptr->header_tx);

    // Clean up shared images
    SDL_DestroyTexture(data_ptr->logo_tex);
    SDL_DestroyTexture(data_ptr->icon_eth);
    SDL_DestroyTexture(data_ptr->icon_lo);
    SDL_DestroyTexture(data_ptr->icon_virt);
    SDL_DestroyTexture(data_ptr->icon_wifi);
    
    // Clean up fonts
    TTF_CloseFont(data_ptr->font);
    TTF_CloseFont(data_ptr->header_font);
}