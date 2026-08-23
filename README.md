![Header Image](https://github.com/ee209-2020class/ee209-2020class.github.io/blob/master/ExtraInfo/logo.png)

# ELECTENG 209 Project Repository

This is a repository to be used by all team members to store your ELECTENG 209 project files.
> [!IMPORTANT]
> Use this repository to store all of your project work, including your LTspice models, PCB design, firmware written with Atmel Studio, Proteus simulations, etc.

You may create as many folders and files as you wish.

To get started, you should:
1. Decide whether your team wishes to use the [advanced structure](./Advanced/) or the [basic structure](./Basic/).
2. Delete the folder that you choose *not* to use.
3. Move the inner folders (excluding the `README.md` file in the chosen `Basic/` or `Advanced/` folder) of your chosen structure to the top-level of the repository.
4. Delete the empty `Basic/` or `Advanced/` folder together with its `README.md` file.

## 🗃 Project Management

A variety of GitHub project management templates have been provided to you, which you may choose to use:

| GitHub Feature                                                          | Potential Application                                                                                                        |
| ----------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| [**Projects**](https://github.com/orgs/uoa-ece209/projects/3?pane=info) | To get a big-picture view of all the tasks within your project, and their statuses.                                          |
| [**Issues**](../../issues)                                              | To track each individual project task.                                                                                       |
| [**Wikis**](../../wiki)                                                 | To store your team contract, meeting agendas/minutes, team logbooks, theoretical calculations, technical documentation, etc. |
| [**Pull Requests**](../../pulls)                                        | To review new changes as a team before they are merged into `main`.                                                          |

- Refer to [`CONTRIBUTING.md`](./.github/CONTRIBUTING.md) for a suggested possible workflow using these features.
- You may also consult the TAs for further advice and guidance.
- See [**❔ Extra Help**](#-extra-help) for places to find extra help.

> [!NOTE]
> You are not limited to just these.  
> You may use hard-copy logbooks instead of digital tools.

## 🎯 Design Specifications

| Parameter                        | Value                                       |
| -------------------------------- | ------------------------------------------- |
| Source Voltage                   | 14 VRMS ± 10%                               |
| Source Frequency                 | 500 Hz ± 2%                                 |
| Load Range                       | 2.5 VA to 7.5 VA                            |
| Load Power Factor                | 0.6 to 0.99                                 |
| Measurement Accuracy             | ±5% of full-scale reading                   |
| ADC Conversion Rate              | 10 kHz or slower                            |
| MCU System Clock                 | 2 MHz                                       |
| LCD Display Information          | Voltage (Vrms), Current (mApeak), Power (W) |
| LCD Scroll Rate                  | 1 s                                         |
| UART Specifications              | 9600 Baud 8N1 with No Parity                |
| Information Transferred Via UART | Voltage, Peak Current, Power and Energy     |
| BLE Functionality                | Optional                                    |
| PCB Size                         | 20000 mm²                                   |
| PCB Technology                   | Double Layer with PTH                       |
| Device Technology                | TH or SMT                                   |

## ❔ Extra Help

For help on using Git and GitHub, you may:
1. Refer to the **`Getting Started with GitHub`** workshop provided on Canvas
2. Ask a question on Slack or Ed Discussion
3. Ask a TA for help
4. Refer to https://git-scm.com/book/en/v2
5. Search the internet!