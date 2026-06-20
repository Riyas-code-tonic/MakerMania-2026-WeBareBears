# MakerMania-2026-WeBareBears 
> Location: MBF Tinkerers' Lab 007
>
> Team Size: 3–5 Students
>
> Goal: Identify a real-world problem and develop an innovative, patentable, and implementable solution.

---

# 1. Team Identity

## 1.1 Team Name : We Bare Bears
<img src="images/THE TEAM.jpg" width="400">

---

## 1.2 Team Members

| Name | Role | YEAR | BRANCH |
| ---- | ---- | ---- | ----   |
| Riya Pailwan    |      | SE     |  ETCS      |
| Ankita Karmakar    |      | FE      |  EXTC      |
| Arsalan Lunje    |      |  SE    |  AURO      |

---

# 2. Problem Discovery

## 2.1 Observation Area

Where did you conduct your observations?

Observations were conducted while travelling as a pillion passenger on a motorcycle in real urban road conditions, directly experiencing the communication difficulties caused by wind noise, engine noise, and surrounding traffic


---

## 2.2 AEIOU Observation Sheet

### Activities

What are users doing?

Riders  turn their heads, rely on shoulder taps, wait for traffic stops, or even pull over to communicate, creating inconvenience and potential safety risks.

### Environment

What conditions affect them?

Communication between riders and pillions is heavily affected by wind, engine, and traffic noise, especially at high speeds where normal conversation becomes difficult. Helmet design, rider-pillion positioning, road vibrations, weather, and high temperatures further reduce speech clarity and make effective communication challenging.

### Interactions
Who or what are they interacting with?

The rider’s attention is focused on operating the motorcycle, making communication difficult and secondary. The pillion usually communicates by shouting or tapping the rider’s shoulder, but communication is one-sided and often hindered by surrounding noise. Existing solutions like phone earbuds do not effectively address rider–pillion communication.

### Objects

What tools or products are used?

At present, most rider–pillion pairs rely on shouting or hand gestures to communicate, as there are no widely used dedicated communication tools. While premium Bluetooth intercom systems exist, they are expensive and require both users to wear compatible helmet-mounted devices. Mobile phones and earbuds are mainly used for music or navigation and do not effectively solve rider–pillion communication.


### Users

Who are the primary users?

The primary users are motorcycle riders who frequently travel with a pillion, including daily commuters, couples, families, students, and touring enthusiasts. These riders often face communication difficulties due to road and wind noise, making a simple and affordable communication solution highly valuable. The rider is the primary purchaser, while the pillion directly benefits from improved communication. By offering an easy-to-use system that does not require both users to wear specialized helmets, the solution targets a broad market and provides a practical, cost-effective alternative to expensive premium intercom systems.



---

## 2.3 Observation Log

| Observation                                                     | Evidence                                                                                                                            | Pain Point                                                                       |
| --------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------- |
| Rider and pillion struggle to communicate while riding.         | I personally faced this issue as a pillion rider when I could not clearly hear what the rider was saying during the journey. | Miscommunication and inability to have clear conversations during the ride.      |
| Riders often use unsafe or inconvenient methods to communicate. | Riders turn their heads, shout, or rely on shoulder taps and hand gestures.                                                         | Reduced riding safety and distraction from the road.                             |
| Existing communication systems are expensive or impractical.    | Premium Bluetooth intercoms require both users to wear compatible helmet devices and can be costly.                                 | Most everyday riders do not have access to an affordable communication solution. |

---

# 3. User Research

## 3.1 Interview Summary

Number of users interviewed: ___3___

## 3.2 Key Quotes

1. "The wind and engine noise make it difficult to hear what the other person is saying while riding."

2. "I often have to repeat myself several times before my message is understood."

3. "I sometimes need to turn back while riding just to communicate with the pillion passenger."

---

## 3.3 User Persona

## *Persona 1*

**Name:** Ashish Waghmare

**Age:** 20

**Occupation:** Student

**Goals:**
Stay connected with the pillion passenger without interrupting the journey.

**Frustrations:**
Having to repeatedly turn back to communicate with the pillion passenger.

**Needs:**
A hands-free communication solution for rider–pillion interaction.



## *Persona 2*

**Name:** Aarush Srivastava

**Age:** 20

**Occupation:** Student 

**Goals:**
To ensure safe riding while also communicating efficientlty

**Frustrations:**
When trying to communicate something specific, riders often have to shout
and repeat themselves multiple times before the pillion can understand the message 

**Needs:** 
Clear and reliable voice transmission while commuting.


## *Persona 3*

**Name:** Ekansh Bansode

**Age:** 19

**Occupation:** Student

**Goals:**
Stay focused on the road while riding.

**Frustrations:**
Pillion passengers needing to lean forward to be heard.

**Needs:**
Comfortable inside the helmet and proper comunication without interference

---

# 4. Problem Framing

## Problem Statement

A motorcycle rider and pillion passenger need a way to communicate clearly during travel because road, wind, and engine noise make verbal communication difficult and unsafe.

---

## How Might We Questions

1. How might we enable clear communication between a motorcycle rider and pillion without requiring both to wear specialized helmets?

2. How might we improve communication without requiring riders to shout or repeat themselves?

3. How might we allow riders to communicate hands-free without distracting them from riding?

---

## Opportunity Ranking

| Criteria         | Score (/10) |
| ---------------- | ----------- |
| Severity         | 8           |
| Frequency        | 8           |
| Feasibility      | 7           |
| Novelty          | 7           |
| Market Potential | 8           |
| **Total**        | **38 / 50** |


---

# 5. Solution Ideation

| Idea                                                                                           | Advantages                                                       | Challenges                                                 |
| ---------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- | ---------------------------------------------------------- |
| **Helmet Communication System** – Enables clear rider-to-pillion communication wirelessly.     | Improves communication, affordable, enhances riding convenience. | Wind noise, Bluetooth integration, and battery management. |
| **Underground Pipe Leak Detection** – Detects hidden water leaks using acoustic sensors.       | Early leak detection, saves water, reduces repair costs.         | Noise filtering and underground deployment complexity.     |
| **Smart Lost Device Finder** – Locates misplaced remotes, earbuds, and other wireless devices. | Saves time, easy to use, reduces frustration.                    | Limited range and dependence on tracking hardware.         |



---

## Selected Concept : Helmet Communication System



Why was this concept chosen?

We chose the **Helmet Communication System** because it addresses a problem we have personally experienced. As a pillion rider, I often found it difficult to hear what the rider was saying due to wind, traffic, and engine noise, making communication frustrating and ineffective. We also observed that many other riders and pillions face the same issue but usually accept it as a normal part of riding rather than seeking a solution. Although communication systems already exist, they typically require **both the rider and the pillion to wear compatible helmet-mounted devices**, making them expensive and impractical for everyday use. Our concept aims to provide a simpler, more affordable solution that enables clear communication without requiring both users to use specialized helmets.



---

# 6. System Design

## High-Level Description

Explain your solution.

---

## Block Diagram

Insert diagram here.
```text

                        ┌───────────────────┐    ┌───────────────────┐    ┌───────────────────┐       ┌───────────────────┐    ┌───────────────────┐
                        │   Voice Input     │───►│  Voice Capture    │───►│ Audio Processing  │─────► │ Audio Received    │───►│   Clear Audio     │
                        │                   │    │                   │    │ & Noise Reduction │    ▲  │   by Earbud       │    │      Heard        │
                        │ Rider / Pillion   │    │ INMP441 MEMS Mic  │    │      ESP32        │    │  │ Bluetooth Earbud  │    │     by User       │
                        │      Speaks       │    │                   │    │                   │    │  │   / Earpiece      │    │                   │
                        └───────────────────┘    └─────────▲─────────┘    └─────────▲─────────┘    │  └───────────────────┘    └───────────────────┘
                                                                                    │              │                                 
                                                                                    │              │                                
                                                                           3.3 V Stable Power      │              
                                                                                    │              │             
                                                                                    │              │               
                        ┌───────────────────┐    ┌───────────────────┐    ┌───────────────────┐    │             
                        │  TP4056 Module    │───►│   Li-Po Battery   │───►│   LDO Regulator   │    │             
                        │ Charging &        │    │ 3.0V - 4.2V       │    │ MCP1700 / XC6206  │    │             
                        │ Protection        │    │ (Nom. 3.7V)       │    │   3.3V Output     │    │              
                        └───────────────────┘    └───────────────────┘    └───────────────────┘    │             
                                                           │                                       │                
                                                           │                                       │            
                                                           │                                       │             
                                              ┌────────────▼────────────┐                          │             
                                              │ RS2597 Bluetooth Module │──────────────────────────│
                                              │    3.7V Direct Power    │ 
                                              │   Audio Transmission    │
                                              │       via Bluetooth     │
                                              └─────────────────────────┘
                                                                                
```                   

## Inputs

List sensors, user inputs, data sources.
|INPUT|SOURCE|
|---|---|
|Rider Voice|INMP441 MEMS Microphone|
|Power Supply|3.7V Li-Po Battery|
---

## Outputs

List displays, actuators, software outputs.
|OITPUT|DEVICE|
|---|---|
|Wireless Audio Signal|RS2597 Bluetooth Audio Transceiver|
|Processed Audio Communication|Bluetooth Earbud|
---

# 7. Technical Planning

## Electronics

|Sr.No. | Component                                               | Purpose |
|--| ----                                               | ------- |
|1.| INMP441 MEMS Microphone                          |Captures the rider's or pillion's voice and converts it into digital audio data using the I2S interface.         |
|2.| ESP32 Dev Kit V1                                 |Acts as the main controller, processes audio data, performs noise filtering, and manages wireless communication.         |
|3.| Bluetooth Audio Transceiver Module (RS2597) / KCX_BT_EMITTER      |Transmits processed audio wirelessly to Bluetooth earbuds.         |
|4.| TP4056 Charging Module                           |Charges and protects the lithium-ion battery from overcharging and over-discharging.         |
|5.| 3.7V Li-Po Battery                |Provides portable power to the entire communication system.         |
|6.| Bluetooth Earbud                                 |Receives and plays the transmitted audio to the user.         |
|7.| 7Semi 5V to 3.3V 1A LDO Low Dropout Regulator                                |Provides a stable 3.3V supply to the ESP32, INMP441, and other low-voltage components.       |

Component List : [Component List](docs/Component%20List.docx)
---

## Software

| Tool | Purpose |
| ---- | ------- |
|      |         |
|      |         |

---

## Mechanical / CAD

Describe fabricated components.

---

# 8. Prototype Development

## Version 1

Description:

Lessons Learned:

---

## Version 2

Description:

Lessons Learned:

---

## Final Prototype

Description:

---

# 9. Testing & Validation

## Testing Plan

| Test | Success Criteria |
| ---- | ---------------- |
|      |                  |
|      |                  |

---

## User Feedback

| User | Feedback | Action Taken |
| ---- | -------- | ------------ |
|      |          |              |

---

# 10. Innovation Assessment

## Existing Solutions

List competing products.

---

## What Makes This Different?

---

## Innovation Score

| Parameter       | Score |
| --------------- | ----- |
| Novelty         |       |
| Technical Depth |       |
| Feasibility     |       |
| Impact          |       |
| Scalability     |       |

---

# 11. Intellectual Property

## Prior Art Search

Patents / Products Found:

---

## Novel Features

1.

2.

3.

---

## Provisional Patent Draft

### Title

### Abstract

### Problem

### Solution

### Claims

---

# 12. Business & Deployment

## Target Users

---

## Estimated Cost

---

## Market Opportunity

---

## Sustainability Considerations

---

# 13. Final Demonstration

## Prototype Images

Insert photos.

---

## Demonstration Video Link

---

## GitHub Repository

---

## Presentation Link

---

# 14. Reflection

## What Worked Well?

---

## What Failed?

---

## Key Learnings

---

## Next Steps

* Patent Filing
* Startup Exploration
* Product Development
* Research Publication
* Competition Submission

---

# 15. Final Deliverables Checklist

* Problem Discovery Complete
* User Interviews Complete
* Persona Created
* Problem Statement Finalized
* System Design Complete
* Prototype Demonstrated
* Testing Completed
* Patent Draft Prepared
* Presentation Submitted
* GitHub Repository Updated

---

# MAKERMANIA FINAL PITCH

Each team will present:

1. Problem
2. User Research
3. Insights
4. Solution
5. Prototype Demo
6. Innovation & Patentability
7. Future Roadmap

Presentation Time: 5 Minutes

Q&A: 3 Minutes
