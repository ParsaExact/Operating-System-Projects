
# Operating Systems  - Computer Assignment 1

| Full Name | Student ID |
| ------------ | -------- | 
| Parsa Daghigh | 810101419 |

Repository link: [https://github.com/ParsaExact/Operating-System-Projects](https://github.com/ParsaExact/Operating-System-Projects)
### Project Entities:

1.  **Organizer (Server)**:
    
    -   Manages player connections and game rooms.
        
    -   Broadcasts game results to all participants.
        
    -   Connection Type: **Connection-Oriented** for managing connections with players and rooms, **Broadcast** for announcing results.
        
2.  **Rooms (Subservers)**:
    
    -   Manage individual games between two players.
        
    -   Handle game logic and player interactions.
        
    -   Report results back to the organizer.
        
    -   Connection Type: **Connection-Oriented** for player connections and game management.
        
3.  **Players (Clients)**:
    
    -   Connect to rooms to play the game.
        
    -   Send choices and receive results.
        
    -   Connection Type: **Connection-Oriented** for connecting to rooms.
    - +-----------+       +----------------+         +------------+
| Organizer |-------|     Rooms       |<------>|   Players   |
|  (Server) |       |  (Subservers)   |         |  (Clients)  |
+-----------+       +----------------+         +------------+
     |                    |     |                      |
     |                    |     |                      |
     | Broadcast          |     |                      |
     +--------------------+     +----------------------+
-   **Organizer to Rooms**: Connection-Oriented (TCP).
    
-   **Rooms to Players**: Connection-Oriented (TCP).
    
-   **Organizer to Players (Results)**: Broadcast.

