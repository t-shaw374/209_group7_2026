![Header Image](https://github.com/ee209-2020class/ee209-2020class.github.io/blob/master/ExtraInfo/logo.png)

# Potential Repository Guidelines

> [!NOTE]
> This is only a **suggested** workflow.  
> You may choose to adopt this fully, partially, or not at all.  
> Speak to a TA if you would like further advice, guidance, or alternative suggestions.

## Standard Workflow

### Getting Started

1. Create a [new **Issue**](../../../issues/new) using the issue template

### Making Progress

2. Track the issue progress in your GitHub [**Project**](../../../projects)

3. If you are working locally, `git pull` any upstream changes

4. Create a new feature branch with an appropriate name
> [!TIP]
> `git switch -c rc-filter`
 
5. Work on that branch, referencing the issue `#issueId` in commit messages as appropriate
> [!TIP]
> `git commit -m "feat(rc-filter): add comparator (#issueId)"`

> [!IMPORTANT]
> Before you commit, ensure you `git pull`.  
> After you commit, ensure you `git push`.
 
6. Once the feature is complete, `git push` your local changes

### Finishing Up

7. Open a [new **Pull Request**](../../../compare) into `main` using the Pull Request template that references the `#issueId` you are closing
	1. Assign yourself to the created **Pull Request**
	2. Request review from the other team members
 
> [!CAUTION]
> If given the option, ensure that the base repository of the pull request is this team repository, and not the template repository.  
> This is because this project repository may be created as a [fork](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/about-forks) of the template.  
> You are not expected to understand this for ELECTENG/COMPSYS 209, but you are welcome to ask a TA if you have any questions.
 
8. Delete the feature branch

9. Move the issue card to `Done` in your Project

## Commit Conventions

- Consider following the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) specification